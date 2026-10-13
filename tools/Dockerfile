# Celestial Engine Mk.V — Universal AI Brain Server
# Lightweight, host-agnostic container for Cloud VPS, Docker, or edge nodes.

FROM python:3.11-slim

# Install system dependencies (ffmpeg for audio conversion, curl for health checks)
RUN apt-get update && apt-get install -y --no-install-recommends \
    ffmpeg \
    curl \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

# Install Python requirements
COPY tools/requirements-brain.txt /app/requirements-brain.txt
RUN pip install --no-cache-dir -r requirements-brain.txt

# Copy brain tools and source
COPY tools/ /app/tools/

ENV PYTHONUNBUFFERED=1
ENV PORT=5005

# Expose UDP port 5005 for binary Protocol v2
EXPOSE 5005/udp

CMD ["python", "tools/ai_brain_server.py"]
