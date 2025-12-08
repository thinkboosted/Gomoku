import tkinter as tk
import time
import os

BOARD_SIZE = 20
CELL_SIZE = 30
OFFSET = 20
WINDOW_SIZE = BOARD_SIZE * CELL_SIZE + OFFSET * 2

class GomokuVisualizer:
    def __init__(self, root, log_file):
        self.root = root
        self.root.title("Gomoku Visualizer - Liskvork")
        self.log_file = log_file

        self.canvas = tk.Canvas(root, width=WINDOW_SIZE, height=WINDOW_SIZE, bg="#e0c090")
        self.canvas.pack()

        self.status_label = tk.Label(root, text="Waiting for game...", font=("Arial", 12))
        self.status_label.pack()

        self.last_mtime = 0
        self.board_state = [['-' for _ in range(BOARD_SIZE)] for _ in range(BOARD_SIZE)]

        self.draw_grid()
        self.refresh()

    def draw_grid(self):
        self.canvas.delete("grid")
        for i in range(BOARD_SIZE):
            # Horizontal lines
            start_x = OFFSET
            end_x = OFFSET + (BOARD_SIZE - 1) * CELL_SIZE
            y = OFFSET + i * CELL_SIZE
            self.canvas.create_line(start_x, y, end_x, y, tags="grid")

            # Vertical lines
            start_y = OFFSET
            end_y = OFFSET + (BOARD_SIZE - 1) * CELL_SIZE
            x = OFFSET + i * CELL_SIZE
            self.canvas.create_line(x, start_y, x, end_y, tags="grid")

            # Coordinates
            self.canvas.create_text(x, 10, text=str(i), font=("Arial", 8), tags="grid")
            self.canvas.create_text(10, y, text=str(i), font=("Arial", 8), tags="grid")

    def parse_log(self):
        if not os.path.exists(self.log_file):
            return False

        try:
            mtime = os.path.getmtime(self.log_file)
            if mtime == self.last_mtime:
                return False
            self.last_mtime = mtime

            with open(self.log_file, 'r') as f:
                lines = f.readlines()

            # Find the last valid board representation
            # A board block is 20 lines of 20 chars
            board_lines = []
            info_line = ""

            # Read from end to finding the last board
            i = len(lines) - 1
            while i >= 0:
                line = lines[i].strip()
                # Check if line looks like a board row (20 chars of -, O, X)
                if len(line) == 20 and all(c in '-OX' for c in line):
                    # Potential end of board, try to grab 20 lines up
                    if i - 19 >= 0:
                        potential_board = lines[i-19 : i+1]
                        valid = True
                        for r in potential_board:
                            if len(r.strip()) != 20 or not all(c in '-OX' for c in r.strip()):
                                valid = False
                                break
                        if valid:
                            board_lines = potential_board
                            # Try to find "Move X:" info above
                            for k in range(i-20, max(-1, i-30), -1):
                                if "Move" in lines[k]:
                                    info_line = lines[k].strip()
                                    if k+1 < len(lines) and "Player" in lines[k+1]:
                                        info_line += " - " + lines[k+1].strip()
                                    break
                            break
                i -= 1

            if board_lines:
                for y, row in enumerate(board_lines):
                    for x, char in enumerate(row.strip()):
                        self.board_state[y][x] = char
                self.status_label.config(text=info_line if info_line else "Game in progress")
                return True

        except Exception as e:
            print(f"Error parsing log: {e}")
        return False

    def draw_stones(self):
        self.canvas.delete("stone")
        for y in range(BOARD_SIZE):
            for x in range(BOARD_SIZE):
                char = self.board_state[y][x]
                if char in ('O', 'X'):
                    cx = OFFSET + x * CELL_SIZE
                    cy = OFFSET + y * CELL_SIZE
                    r = CELL_SIZE // 2 - 2

                    color = "black" if char == 'O' else "white"
                    outline = "white" if char == 'O' else "black"

                    self.canvas.create_oval(cx - r, cy - r, cx + r, cy + r, fill=color, outline=outline, width=2, tags="stone")

                    # Highlight last move? (Requires tracking changes, skipping for simplicity)

    def refresh(self):
        if self.parse_log():
            self.draw_stones()
        self.root.after(500, self.refresh) # Refresh every 500ms

if __name__ == "__main__":
    root = tk.Tk()
    app = GomokuVisualizer(root, "board.log")
    root.mainloop()
