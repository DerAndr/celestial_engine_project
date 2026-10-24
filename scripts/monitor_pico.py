import serial
import time
import sys

def main():
    try:
        s = serial.Serial('COM9', 115200, timeout=0.5)
    except Exception as e:
        print(f"Failed to open COM9: {e}")
        sys.exit(1)

    s.read_all()
    print("=== LIVE COM9 MONITOR ACTIVE: Listening for GP9 button & state changes ===")
    sys.stdout.flush()

    try:
        while True:
            line = s.readline().decode('utf-8', errors='ignore').strip()
            if line:
                t = time.strftime("%H:%M:%S")
                # Highlight button events
                if any(k in line for k in ['QRY', 'QUERY', 'edge', 'LISTENING', 'SPEAKING', 'ORACLE', 'PRESS', 'RELEASE', 'PINS']):
                    print(f"[{t}] >>> {line}")
                elif 'HEARTBEAT' in line:
                    print(f"[{t}] {line}")
                else:
                    print(f"[{t}] {line}")
                sys.stdout.flush()
    except KeyboardInterrupt:
        pass
    finally:
        s.close()
        print("=== MONITOR CLOSED ===")

if __name__ == '__main__':
    main()
