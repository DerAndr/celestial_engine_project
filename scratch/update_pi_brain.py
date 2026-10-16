import subprocess

remote_code = """
import json
p = "/home/andr/celestial_engine_project/tools/brain_config.json"
with open(p, "r", encoding="utf-8") as f:
    d = json.load(f)
d["local_model"] = "qwen2.5:0.5b"
models = d.get("local_models_available", [])
if "qwen2.5:0.5b" not in models:
    models.insert(0, "qwen2.5:0.5b")
if "llama3.2:1b" not in models:
    models.append("llama3.2:1b")
d["local_models_available"] = models
with open(p, "w", encoding="utf-8") as f:
    json.dump(d, f, indent=2, ensure_ascii=False)
print("Config updated.")
"""

cmd = ["ssh", "-o", "BatchMode=yes", "-o", "StrictHostKeyChecking=no", "andr@192.168.178.51", f"python3 -c '{remote_code}' && sudo systemctl restart celestial-brain && systemctl is-active celestial-brain"]
res = subprocess.run(cmd, capture_output=True, text=True)
print(res.stdout)
print(res.stderr)
