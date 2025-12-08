import tkinter as tk
from tkinter import ttk
import os

BOARD_SIZE = 20
CELL_SIZE = 30
OFFSET = 20
WINDOW_SIZE = BOARD_SIZE * CELL_SIZE + OFFSET * 2

class GomokuVisualizer:
    def __init__(self, root, log_file):
        self.root = root
        self.root.title("Gomoku Visualizer - Replay Mode")
        self.log_file = log_file

        # --- UI Layout ---

        # 1. Canvas for Board
        self.canvas = tk.Canvas(root, width=WINDOW_SIZE, height=WINDOW_SIZE, bg="#e0c090")
        self.canvas.pack(side=tk.TOP)

        # 2. Control Panel
        self.controls = tk.Frame(root)
        self.controls.pack(side=tk.BOTTOM, fill=tk.X, padx=10, pady=10)

        # Status info
        self.info_label = tk.Label(self.controls, text="Waiting for data...", font=("Arial", 10, "bold"))
        self.info_label.pack(side=tk.TOP, pady=5)

        # Navigation Frame
        nav_frame = tk.Frame(self.controls)
        nav_frame.pack(side=tk.TOP, fill=tk.X)

        self.btn_prev = tk.Button(nav_frame, text="<< Prev", command=self.go_prev)
        self.btn_prev.pack(side=tk.LEFT)

        self.lbl_move = tk.Label(nav_frame, text="0 / 0")
        self.lbl_move.pack(side=tk.LEFT, expand=True)

        self.btn_next = tk.Button(nav_frame, text="Next >>", command=self.go_next)
        self.btn_next.pack(side=tk.RIGHT)
        
        self.btn_reset = tk.Button(self.controls, text="Reset View", command=self.reset_view, bg="#ffcccc")
        self.btn_reset.pack(side=tk.TOP, pady=2)

        # Slider
        self.slider = tk.Scale(self.controls, from_=0, to=0, orient=tk.HORIZONTAL, command=self.on_slider_change)
        self.slider.pack(side=tk.TOP, fill=tk.X, pady=5)

        # Live Checkbox
        self.live_var = tk.BooleanVar(value=True)
        self.chk_live = tk.Checkbutton(self.controls, text="Live (Auto-follow)", variable=self.live_var)
        self.chk_live.pack(side=tk.TOP)

        # Data State
        self.history = [] # List of tuples: (move_num, info_str, board_grid)
        self.current_index = -1
        self.last_mtime = 0

        self.draw_grid()
        self.refresh_loop()

    def draw_grid(self):
        self.canvas.delete("grid")
        for i in range(BOARD_SIZE):
            # Lines
            start = OFFSET
            end = OFFSET + (BOARD_SIZE - 1) * CELL_SIZE
            pos = OFFSET + i * CELL_SIZE
            self.canvas.create_line(start, pos, end, pos, tags="grid")
            self.canvas.create_line(pos, start, pos, end, tags="grid")

            # Text coordinates
            self.canvas.create_text(pos, 10, text=str(i), font=("Arial", 8), tags="grid")
            self.canvas.create_text(10, pos, text=str(i), font=("Arial", 8), tags="grid")

    def parse_full_log(self):
        if not os.path.exists(self.log_file):
            return

        try:
            mtime = os.path.getmtime(self.log_file)
            if mtime == self.last_mtime:
                return # No file change
            self.last_mtime = mtime

            with open(self.log_file, 'r') as f:
                lines = [line.rstrip() for line in f]

            new_history = []
            i = 0
            while i < len(lines):
                line = lines[i]
                if line.startswith("Move"):
                    # Start of a move block
                    move_info = line
                    player_info = ""
                    if i + 1 < len(lines) and "Player" in lines[i+1]:
                        player_info = lines[i+1]

                    full_info = f"{move_info} - {player_info}"

                    # Look for board start (first line of dashes)
                    board_start = -1
                    for k in range(i, min(i + 10, len(lines))):
                        if len(lines[k]) == 20 and all(c in '-OX' for c in lines[k]):
                            board_start = k
                            break

                    if board_start != -1 and board_start + 20 <= len(lines):
                        board_grid = []
                        valid_board = True
                        for r in range(20):
                            row_str = lines[board_start + r]
                            if len(row_str) != 20:
                                valid_board = False
                                break
                            board_grid.append(list(row_str))

                        if valid_board:
                            new_history.append({
                                'info': full_info,
                                'board': board_grid
                            })
                            i = board_start + 20
                            continue
                i += 1

            # Update history
            if len(new_history) > len(self.history):
                # New moves arrived
                self.history = new_history
                self.slider.config(to=len(self.history) - 1)

                # If in Live mode, jump to end
                if self.live_var.get():
                    self.current_index = len(self.history) - 1
                    self.slider.set(self.current_index)
                    self.update_display()
                else:
                    # Update label limits even if not moving
                    self.update_ui_labels()

        except Exception as e:
            print(f"Log parse error: {e}")

    def update_display(self):
        if not self.history or self.current_index < 0:
            return

        state = self.history[self.current_index]
        self.info_label.config(text=state['info'])

        self.canvas.delete("stone")
        grid = state['board']

        for y in range(BOARD_SIZE):
            for x in range(BOARD_SIZE):
                char = grid[y][x]
                if char in ('O', 'X'):
                    cx = OFFSET + x * CELL_SIZE
                    cy = OFFSET + y * CELL_SIZE
                    r = CELL_SIZE // 2 - 2

                    color = "black" if char == 'O' else "white"
                    outline = "white" if char == 'O' else "black"

                    self.canvas.create_oval(cx - r, cy - r, cx + r, cy + r, fill=color, outline=outline, width=2, tags="stone")

                    # Highlight last played stone logic could go here if we diffed with prev state

        self.update_ui_labels()

    def update_ui_labels(self):
        total = len(self.history)
        current = self.current_index + 1 if total > 0 else 0
        self.lbl_move.config(text=f"{current} / {total}")

    def go_prev(self):
        if self.current_index > 0:
            self.live_var.set(False) # Stop auto-follow
            self.current_index -= 1
            self.slider.set(self.current_index)
            self.update_display()

    def go_next(self):
        if self.current_index < len(self.history) - 1:
            self.current_index += 1
            self.slider.set(self.current_index)
            self.update_display()

    def on_slider_change(self, val):
        idx = int(val)
        if 0 <= idx < len(self.history):
            self.live_var.set(False) # User interaction stops live mode
            self.current_index = idx
            self.update_display()

    def reset_view(self):
        self.history = []
        self.current_index = -1
        self.canvas.delete("stone")
        self.info_label.config(text="History cleared")
        self.lbl_move.config(text="0 / 0")
        self.slider.config(to=0)
        self.slider.set(0)
        # Force re-read of log from scratch next cycle? 
        # Actually, better to just clear memory. Next parse_full_log will re-read file.
        # If file still has old data, it will reappear.
        # Ideally, user should 'rm board.log' then click this, or we handle 'sessions'.
        
    def refresh_loop(self):
        self.parse_full_log()
        self.root.after(500, self.refresh_loop)

if __name__ == "__main__":
    root = tk.Tk()
    app = GomokuVisualizer(root, "board.log")
    root.mainloop()