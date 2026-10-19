"""
Automated Verification Suite: Vercel Rewrite Routing & Authentication
Tests path resolution, WSGI serverless execution, and user authentication.
"""
import io
import json
import os
import sys
import unittest

# Ensure project root is in sys.path
PROJECT_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
if PROJECT_ROOT not in sys.path:
    sys.path.insert(0, PROJECT_ROOT)

from api.index import app, handler, handle_api, resolve_request_path, get_db


class TestVercelRoutingAndAuth(unittest.TestCase):
    def setUp(self):
        # Ensure database is initialized
        self.conn = get_db()

    def test_01_resolve_request_path_direct(self):
        """Direct paths when accessed locally or un-rewritten."""
        self.assertEqual(resolve_request_path("/api/statistics", {}, {}), "/api/statistics")
        self.assertEqual(resolve_request_path("/api/auth/login", {}, {}), "/api/auth/login")
        self.assertEqual(resolve_request_path("/api/items/lost", {}, {}), "/api/items/lost")

    def test_02_resolve_request_path_vercel_query_param(self):
        """Vercel destination rewrites passing __path query param."""
        raw_path = "/api/index.py"
        self.assertEqual(
            resolve_request_path(raw_path, {}, {"__path": "/api/auth/login"}),
            "/api/auth/login"
        )
        self.assertEqual(
            resolve_request_path(raw_path, {}, {"__path": "/api/statistics"}),
            "/api/statistics"
        )
        self.assertEqual(
            resolve_request_path(raw_path, {}, {"path": "items/found"}),
            "/api/items/found"
        )

    def test_03_resolve_request_path_vercel_headers(self):
        """Vercel edge rewrite headers (x-matched-path, x-forwarded-uri, etc.)."""
        raw_path = "/api/index.py"
        self.assertEqual(
            resolve_request_path(raw_path, {"x-matched-path": "/api/auth/login"}, {}),
            "/api/auth/login"
        )
        self.assertEqual(
            resolve_request_path(raw_path, {"X-Matched-Path": "/api/statistics"}, {}),
            "/api/statistics"
        )
        self.assertEqual(
            resolve_request_path(raw_path, {"x-forwarded-uri": "/api/claims"}, {}),
            "/api/claims"
        )

    def test_04_resolve_request_path_root_api_fallback(self):
        """Fallback to /api when targeting the serverless function root."""
        self.assertEqual(resolve_request_path("/api/index.py", {}, {}), "/api")
        self.assertEqual(resolve_request_path("/api/index", {}, {}), "/api")

    def test_05_wsgi_root_api_status(self):
        """GET /api returns 200 OK with health status and endpoint list."""
        status_code = None
        headers = []

        def fake_start_response(status, resp_headers):
            nonlocal status_code, headers
            status_code = int(status.split()[0])
            headers = resp_headers

        environ = {
            "REQUEST_METHOD": "GET",
            "PATH_INFO": "/api",
            "QUERY_STRING": "",
            "wsgi.input": io.BytesIO(b""),
        }
        res_iter = app(environ, fake_start_response)
        body = json.loads(b"".join(res_iter).decode("utf-8"))

        self.assertEqual(status_code, 200)
        self.assertTrue(body.get("success"))
        self.assertEqual(body.get("status"), "healthy")
        self.assertIn("/api/auth/login", body.get("endpoints", []))

    def test_06_wsgi_vercel_rewritten_statistics(self):
        """Simulate Vercel invoking app with PATH_INFO='/api/index.py' and query param."""
        status_code = None

        def fake_start_response(status, resp_headers):
            nonlocal status_code
            status_code = int(status.split()[0])

        environ = {
            "REQUEST_METHOD": "GET",
            "PATH_INFO": "/api/index.py",
            "QUERY_STRING": "__path=/api/statistics",
            "wsgi.input": io.BytesIO(b""),
        }
        res_iter = app(environ, fake_start_response)
        body = json.loads(b"".join(res_iter).decode("utf-8"))

        self.assertEqual(status_code, 200)
        self.assertTrue(body.get("success"))
        self.assertIn("total_lost", body.get("data", {}))

    def test_07_wsgi_admin_login(self):
        """POST /api/auth/login via Vercel rewrite with admin credentials."""
        status_code = None

        def fake_start_response(status, resp_headers):
            nonlocal status_code
            status_code = int(status.split()[0])

        payload = json.dumps({"identifier": "admin", "password": "admin123"}).encode("utf-8")
        environ = {
            "REQUEST_METHOD": "POST",
            "PATH_INFO": "/api/index.py",
            "QUERY_STRING": "__path=/api/auth/login",
            "CONTENT_TYPE": "application/json",
            "CONTENT_LENGTH": str(len(payload)),
            "wsgi.input": io.BytesIO(payload),
        }
        res_iter = app(environ, fake_start_response)
        body = json.loads(b"".join(res_iter).decode("utf-8"))

        self.assertEqual(status_code, 200)
        self.assertTrue(body.get("success"))
        self.assertEqual(body.get("user", {}).get("role"), "admin")
        self.assertEqual(body.get("user", {}).get("student_id"), "admin")

    def test_08_wsgi_admin_login_case_insensitive(self):
        """POST /api/auth/login with uppercase 'ADMIN' credentials."""
        status_code = None

        def fake_start_response(status, resp_headers):
            nonlocal status_code
            status_code = int(status.split()[0])

        payload = json.dumps({"identifier": "ADMIN", "password": "admin123"}).encode("utf-8")
        environ = {
            "REQUEST_METHOD": "POST",
            "PATH_INFO": "/api/index.py",
            "QUERY_STRING": "__path=/api/auth/login",
            "CONTENT_TYPE": "application/json",
            "CONTENT_LENGTH": str(len(payload)),
            "wsgi.input": io.BytesIO(payload),
        }
        res_iter = app(environ, fake_start_response)
        body = json.loads(b"".join(res_iter).decode("utf-8"))

        self.assertEqual(status_code, 200)
        self.assertTrue(body.get("success"))
        self.assertEqual(body.get("user", {}).get("role"), "admin")

    def test_09_wsgi_student_login(self):
        """POST /api/auth/login with student account."""
        status_code = None

        def fake_start_response(status, resp_headers):
            nonlocal status_code
            status_code = int(status.split()[0])

        payload = json.dumps({"identifier": "krishna", "password": ""}).encode("utf-8")
        environ = {
            "REQUEST_METHOD": "POST",
            "PATH_INFO": "/api/index.py",
            "QUERY_STRING": "__path=/api/auth/login",
            "CONTENT_TYPE": "application/json",
            "CONTENT_LENGTH": str(len(payload)),
            "wsgi.input": io.BytesIO(payload),
        }
        res_iter = app(environ, fake_start_response)
        body = json.loads(b"".join(res_iter).decode("utf-8"))

        self.assertEqual(status_code, 200)
        self.assertTrue(body.get("success"))
        self.assertEqual(body.get("user", {}).get("role"), "student")

    def test_10_wsgi_login_invalid_password(self):
        """POST /api/auth/login with wrong admin password returns 401."""
        status_code = None

        def fake_start_response(status, resp_headers):
            nonlocal status_code
            status_code = int(status.split()[0])

        payload = json.dumps({"identifier": "admin", "password": "wrong_password"}).encode("utf-8")
        environ = {
            "REQUEST_METHOD": "POST",
            "PATH_INFO": "/api/index.py",
            "QUERY_STRING": "__path=/api/auth/login",
            "CONTENT_TYPE": "application/json",
            "CONTENT_LENGTH": str(len(payload)),
            "wsgi.input": io.BytesIO(payload),
        }
        res_iter = app(environ, fake_start_response)
        body = json.loads(b"".join(res_iter).decode("utf-8"))

        self.assertEqual(status_code, 401)
        self.assertFalse(body.get("success"))
        self.assertIn("Incorrect password", body.get("error", ""))

    def test_11_wsgi_header_only_rewrite(self):
        """Simulate Vercel rewrite where only HTTP_X_MATCHED_PATH header is present."""
        status_code = None

        def fake_start_response(status, resp_headers):
            nonlocal status_code
            status_code = int(status.split()[0])

        environ = {
            "REQUEST_METHOD": "GET",
            "PATH_INFO": "/api/index.py",
            "QUERY_STRING": "",
            "HTTP_X_MATCHED_PATH": "/api/items/lost",
            "wsgi.input": io.BytesIO(b""),
        }
        res_iter = app(environ, fake_start_response)
        body = json.loads(b"".join(res_iter).decode("utf-8"))

        self.assertEqual(status_code, 200)
        self.assertTrue(body.get("success"))
        self.assertIsInstance(body.get("data"), list)

    def test_12_handler_base_http(self):
        """Verify BaseHTTPRequestHandler handler works with resolved paths."""
        from unittest.mock import MagicMock
        mock_handler = MagicMock(spec=handler)
        mock_handler.path = "/api/index.py?__path=/api/statistics"
        mock_handler.headers = {}
        mock_handler.rfile = io.BytesIO(b"")
        mock_handler.wfile = io.BytesIO()

        # Execute _handle_request directly on the unbound method
        handler._handle_request(mock_handler, "GET")

        mock_handler.send_response.assert_called_with(200)
        written = mock_handler.wfile.getvalue()
        body = json.loads(written.decode("utf-8"))
        self.assertTrue(body.get("success"))
        self.assertIn("total_lost", body.get("data", {}))


if __name__ == "__main__":
    unittest.main(verbosity=2)
