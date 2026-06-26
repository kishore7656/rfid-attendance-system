import cv2
import mediapipe as mp
import numpy as np
from collections import deque

# ---------------- SETUP ----------------
mp_hands = mp.solutions.hands
mp_draw = mp.solutions.drawing_utils

cap = cv2.VideoCapture(0)

canvas = None
prev_x, prev_y = 0, 0

# smoothing buffer (removes shake)
smooth_points = deque(maxlen=5)

mode = "IDLE"

with mp_hands.Hands(
    max_num_hands=1,
    min_detection_confidence=0.8,
    min_tracking_confidence=0.8
) as hands:

    while True:
        success, frame = cap.read()
        if not success:
            break

        frame = cv2.flip(frame, 1)

        if canvas is None:
            canvas = np.zeros_like(frame)

        h, w, _ = frame.shape

        rgb = cv2.cvtColor(frame, cv2.COLOR_BGR2RGB)
        results = hands.process(rgb)

        cx, cy = 0, 0
        fingers = 0

        if results.multi_hand_landmarks:

            hand = results.multi_hand_landmarks[0]

            # ---------------- DRAW 21 LANDMARK DOTS ----------------
            mp_draw.draw_landmarks(
                frame,
                hand,
                mp_hands.HAND_CONNECTIONS,
                mp_draw.DrawingSpec(color=(0,255,0), thickness=2, circle_radius=2),
                mp_draw.DrawingSpec(color=(0,0,255), thickness=2)
            )

            lm = hand.landmark

            # fingertip tracking (index finger)
            cx, cy = int(lm[8].x * w), int(lm[8].y * h)

            # finger count
            tips = [8, 12, 16, 20]
            fingers = 0

            for t in tips:
                if lm[t].y < lm[t - 2].y:
                    fingers += 1

            thumb = lm[4].y < lm[3].y

            # ---------------- MODES ----------------
            if fingers == 1:
                mode = "DRAW"

            elif fingers == 2:
                mode = "SELECT"

            elif fingers == 3:
                mode = "ERASE"

            elif fingers == 4:
                mode = "MOVE"

            else:
                mode = "IDLE"

        # ---------------- SMOOTHING (VERY IMPORTANT) ----------------
        smooth_points.append((cx, cy))
        avg_x = int(sum(p[0] for p in smooth_points) / len(smooth_points))
        avg_y = int(sum(p[1] for p in smooth_points) / len(smooth_points))

        # ---------------- DRAW MODE (SMOOTH LINE) ----------------
        if mode == "DRAW":

            if prev_x == 0 and prev_y == 0:
                prev_x, prev_y = avg_x, avg_y

            cv2.line(canvas, (prev_x, prev_y), (avg_x, avg_y),
                     (255, 255, 255), 4, cv2.LINE_AA)

            prev_x, prev_y = avg_x, avg_y

        else:
            prev_x, prev_y = 0, 0

        # ---------------- ERASER (CIRCLE TOOL) ----------------
        if mode == "ERASE":

            # circular eraser (THIS IS WHAT YOU ASKED)
            cv2.circle(canvas, (avg_x, avg_y), 25, (0, 0, 0), -1)

            # visual circle on screen
            cv2.circle(frame, (avg_x, avg_y), 25, (0, 0, 255), 2)

        # ---------------- SELECT MODE ----------------
        if mode == "SELECT":
            cv2.rectangle(frame,
                          (avg_x - 40, avg_y - 40),
                          (avg_x + 40, avg_y + 40),
                          (0, 255, 0), 2)

        # ---------------- MERGE CANVAS ----------------
        gray = cv2.cvtColor(canvas, cv2.COLOR_BGR2GRAY)
        _, inv = cv2.threshold(gray, 20, 255, cv2.THRESH_BINARY_INV)
        inv = cv2.cvtColor(inv, cv2.COLOR_GRAY2BGR)

        frame = cv2.bitwise_and(frame, inv)
        frame = cv2.bitwise_or(frame, canvas)

        # ---------------- UI ----------------
        cv2.putText(frame, f"MODE: {mode}", (30, 50),
                    cv2.FONT_HERSHEY_SIMPLEX, 1,
                    (0, 255, 0), 2)

        cv2.imshow("21 Point Hand Drawing System", frame)

        key = cv2.waitKey(1) & 0xFF

        # CLEAR
        if key == ord('c'):
            canvas = np.zeros_like(frame)

        if key == 27:
            break

cap.release()
cv2.destroyAllWindows()
