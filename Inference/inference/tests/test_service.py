import asyncio
from dataclasses import replace
import json
import uuid
import httpx

from fastapi.testclient import TestClient
import pytest

from inference.app import create_app
from inference.config import Settings
from inference.store import Store
from inference.backend import Backend


class FakeBackend:
    def __init__(self, settings):
        self.settings = settings
        self.prompts = []
        self.fail = False

    async def ready(self):
        pass

    async def render(self, messages):
        self.prompts.append(messages)
        return list(range(sum(len(m["content"]) for m in messages)))

    async def complete(self, tokens, maximum, temperature):
        yield {"content": "remembered ", "stop": False}
        if self.fail:
            raise ValueError("Simulated backend interruption")
        yield {"content": "answer", "stop": True, "stop_type": "eos"}

    async def summarize(self, previous, turns):
        return "Project code: orchid-123"


@pytest.fixture
def service(tmp_path):
    settings = replace(Settings(), database=str(tmp_path / "memory.db"), api_key="test-key")
    backend = FakeBackend(settings)
    app = create_app(settings, backend)
    with TestClient(app, headers={"Authorization": "Bearer test-key"}) as client:
        yield client, backend, settings


def new(client):
    return client.post("/v1/sessions", json={}).json()["session_id"]


def payload(message="hello", stream=False):
    return {"request_id": str(uuid.uuid4()), "message": message, "stream": stream}


def test_context_persistence_and_idempotency(service):
    client, backend, settings = service
    sid = new(client)
    body = payload("My project code is orchid-123")
    first = client.post(f"/v1/sessions/{sid}/chat", json=body)
    assert first.status_code == 200
    replay = client.post(f"/v1/sessions/{sid}/chat", json=body)
    assert replay.json() == first.json()
    assert len(backend.prompts) == 1
    with TestClient(create_app(settings, backend), headers={"Authorization": "Bearer test-key"}) as restarted:
        assert restarted.post(f"/v1/sessions/{sid}/chat", json=payload("Recall my code")).status_code == 200
        assert backend.prompts[-1][1]["content"] == body["message"]
        assert len(restarted.get(f"/v1/sessions/{sid}").json()["turns"]) == 2
    conflict = dict(body, message="different")
    assert client.post(f"/v1/sessions/{sid}/chat", json=conflict).status_code == 409


def test_seed_is_forwarded_and_cached_retry_does_not_generate(service):
    client, backend, _ = service
    seeds = []
    async def seeded(tokens, maximum, temperature, seed=None, json_schema=None, stop=None):
        seeds.append((seed,json_schema,stop))
        yield {"content": "A seeded reply", "stop": True, "stop_type": "eos"}
    backend.complete = seeded
    sid = new(client)
    schema = {'type': 'object', 'properties': {'reply': {'type': 'string'}}}
    body = dict(payload(), seed=84721, response_schema=schema, stop=['\n\n'])
    first = client.post(f"/v1/sessions/{sid}/chat", json=body)
    assert first.status_code == 200
    assert client.post(f"/v1/sessions/{sid}/chat", json=body).json() == first.json()
    assert seeds == [(84721,schema,['\n\n'])]
    assert client.post(f"/v1/sessions/{sid}/chat", json=dict(payload(), seed=-1)).status_code == 422


def test_stream_failure_rolls_back_and_releases_capacity(service):
    client, backend, _ = service
    sid = new(client)
    backend.fail = True
    body = payload(stream=True)
    response = client.post(f"/v1/sessions/{sid}/chat", json=body)
    events = [json.loads(line) for line in response.text.splitlines()]
    assert events[-1]["type"] == "error"
    assert client.get(f"/v1/sessions/{sid}").json()["turns"] == []
    backend.fail = False
    response = client.post(f"/v1/sessions/{sid}/chat", json=body)
    assert json.loads(response.text.splitlines()[-1])["type"] == "done"
    assert len(client.get(f"/v1/sessions/{sid}").json()["turns"]) == 1


def test_isolation_auth_delete_and_limits(service):
    client, backend, _ = service
    a, b = new(client), new(client)
    assert client.post(f"/v1/sessions/{a}/chat", json=payload("secret")).status_code == 200
    assert client.post(f"/v1/sessions/{b}/chat", json=payload()).status_code == 200
    assert all("secret" not in m["content"] for m in backend.prompts[-1])
    assert client.get(f"/v1/sessions/{a}", headers={"Authorization": "wrong"}).status_code == 401
    assert client.post(f"/v1/sessions/{a}/chat", json=dict(payload(), max_tokens=99999)).status_code == 422
    assert client.delete(f"/v1/sessions/{a}").status_code == 200
    assert client.get(f"/v1/sessions/{a}").status_code == 404
    assert client.get(f"/v1/sessions/{b}").status_code == 200


def test_compaction_budget_and_history_retention(service):
    client, backend, settings = service
    backend.settings = replace(settings, context=2048, output=512)
    sid = new(client)
    for _ in range(4):
        response = client.post(f"/v1/sessions/{sid}/chat", json=payload("x" * 600))
        assert response.status_code == 200
        assert response.json()["prompt_tokens"] + 512 + 32 <= 2048
    history = client.get(f"/v1/sessions/{sid}").json()
    assert len(history["turns"]) == 4
    assert history["summary"] == "Project code: orchid-123"
    assert history["summarized_through"] > 0


def test_oversized_input_does_not_write(service):
    client, _, _ = service
    sid = new(client)
    assert client.post(f"/v1/sessions/{sid}/chat", json=payload("x" * 10000)).status_code == 422
    assert client.get(f"/v1/sessions/{sid}").json()["turns"] == []
    assert client.post(f"/v1/sessions/{sid}/chat", json=payload()).status_code == 200


def test_atomic_commit(tmp_path):
    store = Store(str(tmp_path / "memory.db"))
    sid = store.create("system")
    store.commit(sid, "one", "user", "assistant", "memory", 0, {})
    with pytest.raises(Exception):
        store.commit(sid, "one", "user", "assistant", "wrong memory", 1, {})
    assert store.load(sid)[0]["summary"] == "memory"


def test_simultaneous_retry_only_generates_once(tmp_path):
    async def run():
        settings = replace(Settings(), database=str(tmp_path / "concurrent.db"), api_key="")
        backend = FakeBackend(settings)
        async with httpx.AsyncClient(transport=httpx.ASGITransport(app=create_app(settings, backend)), base_url="http://test") as client:
            sid = (await client.post("/v1/sessions", json={})).json()["session_id"]
            body = payload()
            a, b = await asyncio.gather(*[client.post(f"/v1/sessions/{sid}/chat", json=body) for _ in range(2)])
            assert a.status_code == b.status_code == 200
            assert a.json() == b.json()
            assert len(backend.prompts) == 1
    asyncio.run(run())


def test_cancelled_generation_releases_lock_and_does_not_commit(tmp_path):
    async def run():
        settings = replace(Settings(), database=str(tmp_path / "cancel.db"), api_key="")
        backend = FakeBackend(settings)
        started = asyncio.Event()
        original = backend.complete

        async def waiting(*args):
            started.set()
            await asyncio.Event().wait()
            yield {}

        backend.complete = waiting
        async with httpx.AsyncClient(transport=httpx.ASGITransport(app=create_app(settings, backend)), base_url="http://test") as client:
            sid = (await client.post("/v1/sessions", json={})).json()["session_id"]
            body = payload(stream=True)
            task = asyncio.create_task(client.post(f"/v1/sessions/{sid}/chat", json=body))
            await asyncio.wait_for(started.wait(), 2)
            task.cancel()
            with pytest.raises(asyncio.CancelledError):
                await task
            assert (await client.get(f"/v1/sessions/{sid}")).json()["turns"] == []
            backend.complete = original
            result = await asyncio.wait_for(client.post(f"/v1/sessions/{sid}/chat", json=body), 2)
            assert json.loads(result.text.splitlines()[-1])["type"] == "done"
    asyncio.run(run())


def test_engine_protocol_uses_counted_tokens():
    async def run():
        settings = Settings()
        backend = Backend(settings)
        await backend.client.aclose()

        def handler(request):
            if request.url.path == "/props":
                return httpx.Response(200, json={"default_generation_settings": {"n_ctx": settings.context}, "total_slots": settings.parallel})
            body = json.loads(request.content)
            if request.url.path == "/apply-template":
                assert body["messages"][0]["role"] == "user"
                return httpx.Response(200, json={"prompt": "<bos>hello<assistant>"})
            if request.url.path == "/tokenize":
                assert body["parse_special"] and body["add_special"]
                return httpx.Response(200, json={"tokens": [1, 2, 3]})
            assert request.url.path == "/completion"
            assert body["prompt"] == [1, 2, 3]
            assert body["cache_prompt"] and body["n_predict"] == 50
            assert body['stop'] == ['\n\n']
            return httpx.Response(200, text='data: {"content":"hello","stop":false}\n\ndata: {"content":"","stop":true,"stop_type":"eos"}\n\n')

        backend.client = httpx.AsyncClient(transport=httpx.MockTransport(handler), base_url="http://engine")
        try:
            await backend.ready()
            tokens = await backend.render([{"role": "user", "content": "hello"}])
            events = [e async for e in backend.complete(tokens, 50, 0.7, stop=['\n\n'])]
            assert events[-1]["stop"]
        finally:
            await backend.client.aclose()
    asyncio.run(run())


def test_done_delivery_does_not_hold_session_or_capacity(tmp_path):
    """The consumer deliberately leaves the done event suspended at yield."""
    async def run():
        from inference.app import ChatInput
        settings = replace(Settings(), database=str(tmp_path/'delivery.db'), api_key='', parallel=1, queue_timeout=.1)
        backend = FakeBackend(settings)
        app = create_app(settings, backend)
        store = Store(settings.database)
        sid = uuid.UUID(store.create('system'))
        endpoint = next(route.endpoint for route in app.routes if getattr(route,'path','')=='/v1/sessions/{sid}/chat')
        response = await endpoint(sid,ChatInput(**payload(stream=True)))
        iterator = response.body_iterator
        while True:
            event=json.loads(await anext(iterator))
            if event['type']=='done':break
        # First response still owns an unclosed iterator. A second request must proceed.
        result = await asyncio.wait_for(endpoint(sid,ChatInput(**payload('next turn'))),1)
        assert result['content']=='remembered answer'
        assert len(store.history(str(sid),0,100))==2
        await iterator.aclose()
    asyncio.run(run())


def test_backend_stop_does_not_wait_for_another_chunk(tmp_path):
    async def run():
        from inference.app import ChatInput
        settings=replace(Settings(),database=str(tmp_path/'terminal.db'),api_key='')
        backend=FakeBackend(settings)
        closed=[]
        async def terminal(*args,**kwargs):
            try:
                yield {'content':'finished','stop':True,'stop_type':'eos'}
                await asyncio.Event().wait()
            finally:
                closed.append(True)
        backend.complete=terminal
        app=create_app(settings,backend)
        sid=uuid.UUID(Store(settings.database).create('system'))
        endpoint=next(r.endpoint for r in app.routes if getattr(r,'path','')=='/v1/sessions/{sid}/chat')
        result=await asyncio.wait_for(endpoint(sid,ChatInput(**payload())),1)
        assert result['content']=='finished'
        assert closed==[True]
    asyncio.run(run())
