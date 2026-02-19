from gods.setup import quick_setup
from game.game import game_loop
from game.agents.randomized import Agent_Random

error_seeds = []
for seed in range(200):
    try:
        game = quick_setup(seed)
        agent = Agent_Random()
        game_loop(game, agent)
    except Exception as e:
        error_seeds.append((seed, str(e)[:80]))

print(f'Success: {200 - len(error_seeds)}, Errors: {len(error_seeds)}')
for seed, err in error_seeds:
    print(f'Seed {seed}: {err}')