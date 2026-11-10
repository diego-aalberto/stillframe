import asyncio
import hashlib
import hmac
import json
import logging
from contextlib import aclosing, asynccontextmanager
from uuid import UUID

import httpx
from fastapi import Depends, FastAPI, Header, HTTPException, Query
from fastapi.responses import StreamingResponse
from pydantic import BaseModel, Field

from .backend import Backend
from .config import Settings
from .memory import ContextTooLarge, prepare
from .store import Store

log = logging.getLogger(__name__)


class OwnedStream(StreamingResponse):
    """Release admission capacity even if sending headers fails before iteration."""
    def __init__(self, *args, release, **kwargs):
        self.release = release
        super().__init__(*args, **kwargs)

    async def __call__(self, scope, receive, send):
        try:
            await super().__call__(scope, receive, send)
        finally:
            try:
                await self.body_iterator.aclose()
            finally:
                self.release()


class SessionInput(BaseModel):
    system: str = Field(default="You are a helpful assistant. Answer directly and accurately.", max_length=16000)


class ChatInput(BaseModel):
    request_id: UUID
    message: str = Field(min_length=1, max_length=64000)
    max_tokens: int = Field(default=512, ge=1)
    temperature: float = Field(default=0.7, ge=0, le=2)
    stream: bool = True
    seed: int | None = Field(default=None, ge=0, le=2147483647)
    response_schema: dict | None = None
    stop: list[str] = Field(default_factory=list, max_length=4)


def create_app(settings=None, backend=None):
    settings = settings or Settings()
    store = Store(settings.database)
    engine = backend or Backend(settings)
    slots = asyncio.Semaphore(settings.parallel)
    locks = [asyncio.Lock() for _ in range(256)]

    @asynccontextmanager
    async def lifespan(app):
        try:
            yield
        finally:
            if backend is None:
                await engine.client.aclose()
            await asyncio.to_thread(store.checkpoint)

    app = FastAPI(title="Local inference with persistent memory", lifespan=lifespan)

    async def authorize(authorization: str = Header(default="")):
        if settings.api_key and not hmac.compare_digest(authorization, "Bearer " + settings.api_key):
            raise HTTPException(401, "Invalid API key")

    def lock_for(sid):
        return locks[int.from_bytes(hashlib.sha256(str(sid).encode()).digest()[:2], "big") % len(locks)]

    @app.get("/health")
    async def health():
        return {"status": "alive"}

    @app.get("/ready", dependencies=[Depends(authorize)])
    async def ready():
        try:
            await engine.ready()
        except (httpx.HTTPError, ValueError, KeyError):
            raise HTTPException(503, "Model unavailable or backend configuration mismatch")
        return {"status": "ready", "context_tokens": settings.context}

    @app.post("/v1/sessions", dependencies=[Depends(authorize)], status_code=201)
    async def create_session(body: SessionInput):
        return {"session_id": await asyncio.to_thread(store.create, body.system)}

    @app.get("/v1/sessions/{sid}", dependencies=[Depends(authorize)])
    async def history(sid: UUID, after: int = Query(0, ge=0), limit: int = Query(100, ge=1, le=500)):
        session = await asyncio.to_thread(store.session, str(sid))
        if session is None:
            raise HTTPException(404, "Unknown session")
        return {"session_id": str(sid), "summary": session["summary"], "summarized_through": session["through"],
                "turns": await asyncio.to_thread(store.history, str(sid), after, limit)}

    @app.post("/v1/sessions/{sid}/summary", dependencies=[Depends(authorize)])
    async def summary(sid: UUID):
        lock = lock_for(sid)
        try:
            await asyncio.wait_for(lock.acquire(), settings.queue_timeout)
        except TimeoutError:
            raise HTTPException(429, "Session busy")
        acquired = False
        try:
            if await asyncio.to_thread(store.session, str(sid)) is None:
                raise HTTPException(404, "Unknown session")
            note = await asyncio.to_thread(store.note, str(sid))
            turns = await asyncio.to_thread(store.history, str(sid), note["through"], 32)
            if turns:
                try:
                    await asyncio.wait_for(slots.acquire(), settings.queue_timeout)
                    acquired = True
                except TimeoutError:
                    raise HTTPException(429, "Inference capacity busy")
                async with asyncio.timeout(settings.timeout):
                    while turns:
                        count = len(turns)
                        text = None
                        while count:
                            text = await engine.summarize(note["summary"], turns[:count])
                            if text is not None:
                                break
                            count //= 2
                        if text is None:
                            raise HTTPException(422, "An archived turn is too large to summarize")
                        note = {"summary": text, "through": turns[count - 1]["seq"]}
                        # Incremental checkpoints make retrying long archives inexpensive.
                        store.save_note(str(sid), **note)
                        turns = await asyncio.to_thread(store.history, str(sid), note["through"], 32)
            return {"session_id": str(sid), **note}
        except (httpx.HTTPError, ValueError, TimeoutError):
            raise HTTPException(502, "Summary failed; earlier summary checkpoints remain saved")
        finally:
            if acquired:
                slots.release()
            lock.release()

    @app.delete("/v1/sessions/{sid}", dependencies=[Depends(authorize)])
    async def delete(sid: UUID):
        lock = lock_for(sid)
        try:
            await asyncio.wait_for(lock.acquire(), settings.queue_timeout)
        except TimeoutError:
            raise HTTPException(429, "Session busy")
        try:
            if not await asyncio.to_thread(store.delete, str(sid)):
                raise HTTPException(404, "Unknown session")
            return {"deleted": True}
        finally:
            lock.release()

    @app.post("/v1/sessions/{sid}/chat", dependencies=[Depends(authorize)])
    async def chat(sid: UUID, body: ChatInput):
        if body.max_tokens > settings.output:
            raise HTTPException(422, f"max_tokens must be <= {settings.output}")
        lock = lock_for(sid)
        acquired = False
        handed_off = False
        released = False
        def release():
            nonlocal released
            if not released:
                released = True
                if acquired:
                    slots.release()
                lock.release()
        try:
            await asyncio.wait_for(lock.acquire(), settings.queue_timeout)
        except TimeoutError:
            raise HTTPException(429, "Session busy", headers={"Retry-After": "2"})
        try:
            cached = await asyncio.to_thread(store.cached, str(sid), str(body.request_id))
            if cached:
                if cached[0] != body.message:
                    raise HTTPException(409, "request_id already used for another message")
                if body.stream:
                    async def replay():
                        yield json.dumps({"type": "done", **cached[1]}) + "\n"
                    return StreamingResponse(replay(), media_type="application/x-ndjson")
                return cached[1]
            loaded = await asyncio.to_thread(store.load, str(sid))
            if loaded is None:
                raise HTTPException(404, "Unknown session")
            try:
                await asyncio.wait_for(slots.acquire(), settings.queue_timeout)
                acquired = True
            except TimeoutError:
                raise HTTPException(429, "Inference capacity busy", headers={"Retry-After": "2"})
            await engine.ready()

            async def events():
                try:
                    async with asyncio.timeout(settings.timeout):
                        tokens, memory = await prepare(engine, *loaded, body.message, body.max_tokens)
                        answer = ""
                        final = None
                        options = {}
                        if body.stop:
                            options['stop'] = body.stop
                        if body.seed is not None:
                            options['seed'] = body.seed
                        if body.response_schema is not None:
                            options['json_schema'] = body.response_schema
                        generation = engine.complete(tokens, body.max_tokens, body.temperature, **options)
                        async with aclosing(generation) as generated:
                            async for event in generated:
                                chunk = event.get("content", "")
                                answer += chunk
                                if chunk:
                                    yield {"type": "token", "content": chunk}
                                if event.get("stop"):
                                    final = event
                                    break
                        if final is None or final.get("truncated") or not answer.strip():
                            raise ValueError("Incomplete or truncated backend response")
                        result = {"session_id": str(sid), "request_id": str(body.request_id),
                                  "content": answer, "prompt_tokens": len(tokens),
                                  "finish_reason": final.get("stop_type", "eos"),
                                  "timings": final.get("timings", {}),
                                  "summarized_through": memory["through"]}
                        # Commit synchronously: tiny transaction must finish before releasing session lock.
                        store.commit(str(sid), str(body.request_id), body.message, answer,
                                     memory["summary"], memory["through"], result)
                        # Delivery/animation must not retain admission after generation is committed.
                        release()
                        yield {"type": "done", **result}
                except ContextTooLarge as exc:
                    release()
                    yield {"type": "error", "code": "context_too_large", "detail": str(exc)}
                except (httpx.HTTPError, ValueError, KeyError, TimeoutError):
                    log.exception("Generation failed")
                    release()
                    yield {"type": "error", "code": "generation_failed", "detail": "Generation failed; retry using the same request_id."}
                finally:
                    release()

            if body.stream:
                async def stream():
                    async with aclosing(events()) as source:
                        async for event in source:
                            yield json.dumps(event, ensure_ascii=False) + "\n"
                response = OwnedStream(stream(), release=release, media_type="application/x-ndjson",
                                             headers={"Cache-Control": "no-cache", "X-Accel-Buffering": "no"})
                handed_off = True
                return response
            handed_off = True
            outcome = None
            async for event in events():
                outcome = event
            if outcome["type"] == "error":
                raise HTTPException(422 if outcome["code"] == "context_too_large" else 502, outcome["detail"])
            return {k: v for k, v in outcome.items() if k != "type"}
        except (httpx.HTTPError, ValueError, KeyError):
            raise HTTPException(503, "Model unavailable or backend configuration mismatch")
        finally:
            if not handed_off:
                release()

    return app
