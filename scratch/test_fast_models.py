import subprocess
import json

models_to_test = ["qwen2.5:0.5b", "llama3.2:1b", "qwen2.5:1.5b"]

for m in models_to_test:
    script = f'''
import urllib.request, json, time
t0 = time.time()
req = urllib.request.Request("http://127.0.0.1:11434/api/generate", 
    data=json.dumps({{"model": "{m}", "prompt": "Ответь одним кратким предложением: в чем смысл жизни?", "stream": False}}).encode(),
    headers={{"Content-Type": "application/json"}})
with urllib.request.urlopen(req) as resp:
    res = json.loads(resp.read().decode())
    dt = time.time() - t0
    txt = res.get("response", "").strip().replace("\\n", " ")
    print(f"[{m}] {{dt:.2f}}s: {{txt}}")
'''
    res = subprocess.run(["ssh", "-o", "BatchMode=yes", "-o", "StrictHostKeyChecking=no", "andr@192.168.178.51", f"python3 -c '{script}'"], capture_output=True, text=True)
    print(res.stdout.strip())
