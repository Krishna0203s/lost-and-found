"""
Campus Lost & Found System — Main Application Entrypoint
Provides top-level 'app' callable for Vercel automatic entrypoint detection.
"""
from index import app, handler

__all__ = ["app", "handler"]

if __name__ == "__main__":
    import sys
    from wsgiref.simple_server import make_server
    port = int(sys.argv[1]) if len(sys.argv) > 1 else 8080
    print(f"Starting server on http://localhost:{port}/")
    with make_server("0.0.0.0", port, app) as httpd:
        httpd.serve_forever()
