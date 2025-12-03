FROM python:3.11-slim

WORKDIR /app

COPY requirements.txt ./
RUN pip install --no-cache-dir -r requirements.txt

COPY firelord_logger ./firelord_logger

ENV PYTHONUNBUFFERED=1

CMD ["python", "-m", "firelord_logger"]
