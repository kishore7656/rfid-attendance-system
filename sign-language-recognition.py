import cv2
import mediapipe as mp
import pyttsx3
import threading
import time
from collections import deque, Counter

# ---------------- Speech ----------------
def speak(text):
    engine = pyttsx3.init()
    engine.say(text)
    engine.runAndWait()

# ---------------- Setup ----------------
mp_hands = mp.solutions.hands
mp_draw = mp.solutions.drawing_utils

cap = cv2.VideoCapture(0)

gesture_history = deque(maxlen=15)

last_spoken = ""
last_time = 0
COOLDOWN = 1.5

with mp_hands.Hands(
    min_detection_confidence=0.8,
    min_tracking_confidence=0.8
) as hands:

    while True:
        success, frame = cap.read()
        if not success:
            break

        frame = cv2.flip(frame, 1)
        rgb = cv2.cvtColor(frame, cv2.COLOR_BGR2RGB)

        results = hands.process(rgb)

        gesture = "UNKNOWN"

        if results.multi_hand_landmarks:

            for hand in results.multi_hand_landmarks:
                mp_draw.draw_landmarks(frame, hand, mp_hands.HAND_CONNECTIONS)

                lm = hand.landmark

                tips = [8, 12, 16, 20]
                fingers_up = 0

                for tip in tips:
                    if lm[tip].y < lm[tip - 2].y:
                        fingers_up += 1

                thumb_up = lm[4].y < lm[3].y

                # ---------------- GESTURES ----------------

                # YES (thumb up only)
                if fingers_up == 0 and thumb_up:
                    gesture = "YES"

                # NO (fist)
                elif fingers_up == 0 and not thumb_up and \
                     lm[8].y > lm[6].y and lm[12].y > lm[10].y and \
                     lm[16].y > lm[14].y and lm[20].y > lm[18].y:
                    gesture = "NO"

                # HELLO (open palm)
                elif fingers_up >= 4 and thumb_up:
                    gesture = "HELLO"

                # ✌️ PEACE
                elif fingers_up == 2 and \
                     lm[8].y < lm[6].y and lm[12].y < lm[10].y and \
                     lm[16].y > lm[14].y and lm[20].y > lm[18].y:
                    gesture = "PEACE"

                # 👍 OK (NOW SIMPLIFIED)
                elif fingers_up == 4:
                    gesture = "OK"

                # 🙏 THANK YOU
                elif fingers_up == 3:
                    gesture = "THANK YOU"

                else:
                    gesture = "UNKNOWN"

        # ---------------- STABILITY ----------------
        gesture_history.append(gesture)

        most_common, count = Counter(gesture_history).most_common(1)[0]
        confidence = count / len(gesture_history)

        cv2.putText(frame, f"{most_common} ({confidence:.2f})",
                    (50, 80),
                    cv2.FONT_HERSHEY_SIMPLEX,
                    1.5,
                    (0, 255, 0),
                    3)

        # ---------------- SPEECH CONTROL ----------------
        current_time = time.time()

        if gesture == "UNKNOWN":
            gesture_history.clear()
            last_spoken = ""

        if confidence > 0.75 and most_common != "UNKNOWN" and \
           (current_time - last_time > COOLDOWN):

            if most_common != last_spoken:

                print("Speaking:", most_common)

                threading.Thread(target=speak, args=(most_common,), daemon=True).start()

                last_spoken = most_common
                last_time = current_time

        # ---------------- DISPLAY ----------------
        cv2.imshow("Sign Language Recognition", frame)

        if cv2.waitKey(1) & 0xFF == 27:
            break

cap.release()
cv2.destroyAllWindows()
