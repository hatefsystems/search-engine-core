import asyncio
import collections
import concurrent.futures
import logging
import os
import sqlite3
import threading
import time
from contextlib import asynccontextmanager
from fastapi import FastAPI, HTTPException, Query, Request
from fastapi.responses import JSONResponse, Response
from pydantic import BaseModel, ConfigDict, Field
from .catalog import Catalog
from .matching import Matcher
from .svg import valid_id


class SuggestionRequest(BaseModel):
    model_config = ConfigDict(extra="forbid")
    text: str = Field(default="", max_length=2000)
    title: str = Field(default="", max_length=200)
    description: str = Field(default="", max_length=2000)
    category: str = Field(default="", max_length=80, pattern=r"^[a-z0-9-]*$")
    context: str = Field(default="", max_length=32)
    preferredSource: str = Field(
        default="", pattern=r"^(lucide|iconify|simple-icons)?$"
    )
    preferredStyle: str = Field(default="", pattern=r"^(outline|filled|mixed|brand)?$")
    limit: int = Field(default=5, ge=1, le=20)


class Limits:
    def __init__(self, app):
        self.app = app
        self.clients = collections.OrderedDict()
        self.rate = int(os.getenv("ICON_RATE_PER_MINUTE", "120"))

    async def __call__(self, scope, receive, send):
        if (
            scope["type"] != "http"
            or scope["path"].startswith("/health/")
            or scope["path"] == "/metrics"
        ):
            return await self.app(scope, receive, send)
        headers = dict(scope["headers"])
        client = headers.get(b"x-icon-client", str(scope.get("client")).encode())[:128]
        now = time.monotonic()
        tokens, last = self.clients.pop(client, (self.rate, now))
        tokens = min(self.rate, tokens + (now - last) * self.rate / 60)
        self.clients[client] = (max(0, tokens - 1), now)
        if len(self.clients) > 4096:
            self.clients.popitem(last=False)
        if tokens < 1:
            return await JSONResponse(
                {"error": "rate_limited"}, 429, headers={"Retry-After": "1"}
            )(scope, receive, send)
        body = b""
        while True:
            message = await receive()
            if message["type"] == "http.disconnect":
                return
            chunk = message.get("body", b"")
            if len(body) + len(chunk) > 16384:
                return await JSONResponse({"error": "body_too_large"}, 413)(
                    scope, receive, send
                )
            body += chunk
            if not message.get("more_body"):
                break
        sent = False

        async def bounded_receive():
            nonlocal sent
            if not sent:
                sent = True
                return {"type": "http.request", "body": body, "more_body": False}
            return await receive()

        return await self.app(scope, bounded_receive, send)


def create_app(root=None, model_dir=None, vector_dir=None):
    @asynccontextmanager
    async def lifespan(app):
        state = app.state
        state.catalog = None
        state.matcher = None
        state.model_status = "disabled"
        state.calls = 0
        state.seconds = 0.0
        state.timeouts = 0
        state.slot = threading.BoundedSemaphore(1)
        state.executor = concurrent.futures.ThreadPoolExecutor(max_workers=1)
        try:
            cat = Catalog(root or os.getenv("ICON_DATA_DIR", "data"))
            state.catalog = cat
            model = vectors = None
            if os.getenv("ICON_SEMANTIC_ENABLED", "false").lower() == "true":
                try:
                    from .embeddings import E5, Vectors

                    model = E5(
                        model_dir or os.getenv("ICON_MODEL_DIR", "models"),
                        int(os.getenv("ICON_THREADS", "1")),
                    )
                    vectors = Vectors(
                        vector_dir or os.getenv("ICON_VECTOR_DIR", "vectors"),
                        cat,
                        model,
                    )
                    state.model_status = "ready"
                except Exception:
                    model = vectors = None
                    state.model_status = "unavailable"
                    logging.warning(
                        "Semantic assets unavailable; lexical fallback active"
                    )
            state.matcher = Matcher(cat, model, vectors)
        except (OSError, ValueError, sqlite3.Error):
            logging.warning("Catalog unavailable; readiness false")
        yield
        state.executor.shutdown(wait=True, cancel_futures=True)

    app = FastAPI(
        title="Hatef Semantic Icon Engine",
        version="1.0.0",
        lifespan=lifespan,
        docs_url=None,
        redoc_url=None,
    )
    app.add_middleware(Limits)

    @app.middleware("http")
    async def security(request, call_next):
        response = await call_next(request)
        response.headers["X-Content-Type-Options"] = "nosniff"
        response.headers["Content-Security-Policy"] = "default-src 'none'; sandbox"
        return response

    def catalog():
        if app.state.catalog is None or not app.state.catalog.count:
            raise HTTPException(503, "catalog_unavailable")
        return app.state.catalog

    @app.get("/health/live")
    def live():
        return {"status": "live"}

    @app.get("/health/ready")
    def ready():
        cat = catalog()
        return {
            "status": "ready",
            "catalogVersion": cat.manifest["version"],
            "icons": cat.count,
            "model": app.state.model_status,
            "mode": "hybrid" if app.state.model_status == "ready" else "lexical",
        }

    @app.get("/metrics")
    def metrics():
        return Response(
            f"icon_suggestions_total {app.state.calls}\nicon_suggestion_seconds_total {app.state.seconds}\nicon_timeouts_total {app.state.timeouts}\n",
            media_type="text/plain",
        )

    @app.get("/v1/icons/categories")
    def categories():
        with catalog().db() as db:
            return {
                "categories": [
                    dict(r)
                    for r in db.execute(
                        "SELECT category,count(*) AS count FROM icons WHERE category!='' GROUP BY category ORDER BY category"
                    )
                ]
            }

    @app.get("/v1/icons/sources")
    def sources():
        import json

        with catalog().db() as db:
            return {
                "sources": [
                    {"id": r[0], **json.loads(r[1])}
                    for r in db.execute("SELECT * FROM sources ORDER BY id")
                ]
            }

    @app.get("/v1/icons")
    @app.get("/v1/icons/search")
    def search(
        q: str = Query("", max_length=500),
        category: str = Query("", max_length=80),
        source: str = Query("", max_length=32),
        style: str = Query("", max_length=32),
        limit: int = Query(20, ge=1, le=100),
        offset: int = Query(0, ge=0, le=1000000),
    ):
        items = catalog().search(q, category, source, style, limit + 1, offset)
        return {
            "items": items[:limit],
            "nextOffset": offset + limit if len(items) > limit else None,
            "catalogVersion": catalog().manifest["version"],
        }

    @app.post("/v1/icons/suggest")
    async def suggest(body: SuggestionRequest):
        catalog()
        text = " ".join((body.text, body.title, body.description)).strip()
        if not text:
            raise HTTPException(422, "text_required")
        if not app.state.slot.acquire(blocking=False):
            raise HTTPException(429, "inference_busy", headers={"Retry-After": "1"})
        start = time.monotonic()
        args = body.model_dump(exclude={"title", "description"})
        args["text"] = text[:4000]
        future = app.state.executor.submit(app.state.matcher.suggest, **args)
        future.add_done_callback(lambda _: app.state.slot.release())
        try:
            result = await asyncio.wait_for(
                asyncio.shield(asyncio.wrap_future(future)),
                timeout=float(os.getenv("ICON_TIMEOUT_SECONDS", "2")),
            )
            app.state.calls += 1
            app.state.seconds += time.monotonic() - start
            return result
        except asyncio.TimeoutError:
            app.state.timeouts += 1
            result = await asyncio.to_thread(Matcher(app.state.catalog).suggest, **args)
            result["reason"] = "semantic_timeout"
            return result

    @app.get("/v1/icons/{icon_id}")
    def detail(icon_id: str):
        if not valid_id(icon_id):
            raise HTTPException(400, "invalid_icon_id")
        result = catalog().get(icon_id)
        if not result:
            raise HTTPException(404, "icon_not_found")
        return result

    @app.get("/assets/icons/{path:path}")
    def svg(path: str, request: Request):
        if not path.endswith(".svg"):
            raise HTTPException(404)
        icon_id = path[:-4].replace("/", ":")
        if not valid_id(icon_id):
            raise HTTPException(400, "invalid_icon_id")
        with catalog().db() as db:
            row = db.execute(
                "SELECT a.svg,a.hash FROM icons i JOIN assets a ON a.hash=i.hash WHERE i.id=?",
                (icon_id,),
            ).fetchone()
        if not row:
            raise HTTPException(404)
        headers = {
            "ETag": '"' + row["hash"] + '"',
            "Cache-Control": "public, max-age=3600",
            "Content-Security-Policy": "default-src 'none'; sandbox",
            "X-Content-Type-Options": "nosniff",
        }
        if request.headers.get("if-none-match") == headers["ETag"]:
            return Response(status_code=304, headers=headers)
        return Response(row["svg"], media_type="image/svg+xml", headers=headers)

    return app


app = create_app()
