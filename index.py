"""
Campus Lost & Found — Root Application Entrypoint
Imports from the self-contained api.index module and provides local testing & runner.
"""
import sys
import os

from api.index import app, handler, handle_api, calculate_match_score

__all__ = ["app", "handler", "handle_api", "calculate_match_score"]

if __name__ == "__main__":
    from wsgiref.simple_server import make_server
    port = int(sys.argv[1]) if len(sys.argv) > 1 else 8080
    print("==================================================")
    print(" Campus Lost & Found — Local Python Server")
    print(f" Running at: http://localhost:{port}/")
    print("==================================================")
    with make_server("0.0.0.0", port, app) as httpd:
        try:
            httpd.serve_forever()
        except KeyboardInterrupt:
            print("\nShutting down server.")
