from __future__ import annotations
import multiprocessing as mp


def _compute(agent, state, choice, pipe):
    """Run in a forked process. Sends back the chosen action index."""
    index = agent.choose_action(state, choice)
    pipe.send(index)
    pipe.close()


class Agent_Process:
    """Wraps any agent so its choose_action runs in a separate process.

    This avoids GIL contention with the main rendering thread.
    Uses fork so that the game state (including lambdas in Choice)
    is inherited without pickling.
    Returns -1 each frame until the result is ready, keeping the render loop responsive.
    """
    def __init__(self, agent):
        self.agent = agent
        self.ctx = mp.get_context("fork")
        self._process = None
        self._pipe = None

    def message(self, msg: str):
        self.agent.message(msg)

    def choose_action(self, state, choice) -> int:
        # Spawn the worker process on the first call for this choice.
        if self._process is None:
            parent_conn, child_conn = self.ctx.Pipe(duplex=False)
            self._pipe = parent_conn
            self._process = self.ctx.Process(
                target=_compute,
                args=(self.agent, state, choice, child_conn),
            )
            self._process.start()
            # Close the child's end in the parent so recv() can detect EOF.
            child_conn.close()

        # Non-blocking check: return -1 if the result isn't ready yet.
        if not self._pipe.poll(timeout=0):
            return -1

        result = self._pipe.recv()
        self._pipe.close()
        self._process.join()
        self._process = None
        self._pipe = None
        return result
