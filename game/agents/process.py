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
    """
    def __init__(self, agent):
        self.agent = agent
        self.ctx = mp.get_context("fork")

    def message(self, msg: str):
        self.agent.message(msg)

    def choose_action(self, state, choice) -> int:
        parent_conn, child_conn = self.ctx.Pipe(duplex=False)
        process = self.ctx.Process(
            target=_compute,
            args=(self.agent, state, choice, child_conn),
        )
        process.start()
        # Close the child's end in the parent so recv() can detect EOF.
        child_conn.close()

        # Poll with short timeout, yielding the GIL between checks.
        while not parent_conn.poll(timeout=0.05):
            if not process.is_alive():
                break

        result = parent_conn.recv()
        parent_conn.close()
        process.join()
        return result
