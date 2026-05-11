# Project
A python project that uses Raylib to implement a simple 2D graphical application to implement cards games.
The system should be able to render cards (rounded rectangles with images and text), handle user input (dragging cards) and manage game state card stacks/decks.

Prefer snake_case for variable and function names. Snake_Case for classes.
Prefer the use of dataclasses.
Prefer functions over methods.
Prioritize simplicity over everything else.
Use comments to explain non-obvious parts of the code.
Separate concerns clerly, using different files and non-intrusive APIs (rendering, input handling, game state management).
For hardcoded constants, fetch them from the "tweak" dictionary  defined in `config.py`. Edit that file to add new constants.