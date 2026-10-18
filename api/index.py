"""
Vercel Serverless Function Entrypoint (/api)
Directs serverless invocations to the core application handler.
"""
import sys
import os

# Add root directory to module search path
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from index import app, handler

__all__ = ["app", "handler"]
