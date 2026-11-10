"""Reap this engine's process tree if its owning supervisor disappears."""
import sys
import time
import psutil


def matching(pid, created):
    try:
        p = psutil.Process(int(pid))
        return p if abs(p.create_time() - float(created)) < 0.01 and p.is_running() else None
    except psutil.Error:
        return None


def terminate_tree(process, timeout=5):
    try:
        children = process.children(recursive=True)
    except psutil.Error:
        children = []
    targets = children + [process]
    for target in targets:
        try:
            target.terminate()
        except psutil.Error:
            pass
    _, alive = psutil.wait_procs(targets, timeout=timeout)
    for target in alive:
        try:
            target.kill()
        except psutil.Error:
            pass
    psutil.wait_procs(alive, timeout=timeout)


def main():
    owner_pid, owner_time, child_pid, child_time = sys.argv[1:]
    while engine := matching(child_pid, child_time):
        if not matching(owner_pid, owner_time):
            terminate_tree(engine)
            return
        time.sleep(0.5)


if __name__ == '__main__':
    main()
