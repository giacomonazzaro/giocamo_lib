./tressette/build/tressette_selfplay --num-games=1000 --depth=6 --samples=20 --out=tressette/selfplay_data.bin

python tressette/train_value.py --data tressette/selfplay_data.bin --out tressette/tressette_value.pt