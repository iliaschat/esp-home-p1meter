FROM mcr.microsoft.com/devcontainers/python:3.12-bookworm

RUN python -m venv /opt/esphome-venv \
    && /opt/esphome-venv/bin/pip install --no-cache-dir esphome==2026.10.2

RUN usermod -aG dialout vscode

ENV PATH="/opt/esphome-venv/bin:${PATH}"