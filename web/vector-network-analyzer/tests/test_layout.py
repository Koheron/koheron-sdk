"""Check workspace geometry throughout sweeps in real Chromium."""
from functools import partial
from http.server import ThreadingHTTPServer
from threading import Thread
import sys
from playwright.sync_api import sync_playwright
from test_browser import QuietHandler, WEB, BOARD


def run():
    server = ThreadingHTTPServer(("127.0.0.1", 0), partial(QuietHandler, directory=str(WEB)))
    Thread(target=server.serve_forever, daemon=True).start()
    protocol = "--protocol" in sys.argv
    url = f"http://127.0.0.1:{server.server_port}/" + ("" if protocol else "?demo=1")
    selectors = [".measurement-toolbar", ".command-bar", "#run", "#stop", "#repeat",
                 "#view-phase", "#reset-view", "#progress-panel", "#progress-title",
                 "#sweep-progress", "#progress-percent", ".status-detail", "#magnitude-plot", "#phase-plot",
                 ".spectrum-readouts", "#measurement-summary", "#sweep-timing",
                 "#hover-reading", ".sidebar", "#marker-0", "#marker-0-gain",
                 "#marker-0-phase", "#marker-0-delay"]
    failures = []
    with sync_playwright() as p:
        browser = p.chromium.launch(args=["--no-sandbox"])
        try:
            widths = [1440, 768, 390] if protocol else [1440, 1200, 1100, 1099, 900, 768, 601, 600, 390, 320]
            sizes = [(width, 900) for width in widths]
            if not protocol:
                sizes += [(1360, 768), (1100, 768)]
            for width, height in sizes:
                page = browser.new_page(viewport={"width": width, "height": height})
                page.goto(url)
                page.wait_for_function("window.vna && !document.querySelector('#run').disabled")
                if protocol:
                    page.locator("#sweep-duration").select_option("0.1")
                page.evaluate("document.fonts.ready")
                page.evaluate("""selectors => {
                    const bounds = () => selectors.map(selector => {
                        const r = document.querySelector(selector).getBoundingClientRect();
                        return [r.x, r.y, r.width, r.height];
                    });
                    const initial = bounds();
                    window.layoutProbe = {running: true, movement: {}};
                    const sample = () => {
                        bounds().forEach((rect, i) => {
                            const delta = Math.max(...rect.map((v, j) => Math.abs(v - initial[i][j])));
                            if (delta > .5) layoutProbe.movement[selectors[i]] = Math.max(delta, layoutProbe.movement[selectors[i]] || 0);
                        });
                        if (layoutProbe.running) requestAnimationFrame(sample);
                    };
                    requestAnimationFrame(sample);
                }""", selectors)
                # First result, subsequent repeat cycles, then cancellation.
                page.locator("#repeat").check()
                page.locator("#run").click()
                for _ in range(3):
                    old = page.evaluate("vna.model.latest?.timestamp")
                    page.wait_for_function("old => vna.model.latest && vna.model.latest.timestamp !== old", arg=old)
                page.locator("#stop").click()
                page.wait_for_function("!document.querySelector('#run').disabled")
                if protocol:
                    previous_timing = page.locator("#sweep-timing").inner_text()
                    assert previous_timing
                    page.locator("#run").click()
                    assert page.locator("#sweep-timing").inner_text() == previous_timing
                    page.wait_for_function("!document.querySelector('#run').disabled", timeout=30000)
                # Validation messages and clearing them must leave the plots in place.
                page.locator("#source-level").fill("")
                page.locator("#source-level").press("Tab")
                page.locator("#run").click()
                page.locator("#source-level").fill("-6")
                page.locator("#source-level").press("Tab")
                page.locator("#run").click()
                page.wait_for_function("!document.querySelector('#run').disabled")
                # Hover replaces the hint with measured values.
                box = page.locator("#magnitude-plot").bounding_box()
                page.mouse.move(box["x"] + box["width"] * .6, box["y"] + 80)
                page.wait_for_timeout(100)
                if width >= 1100:
                    # Expanding notes scrolls the sidebar without resizing plots.
                    page.locator(".preferences-panel summary").click()
                    page.locator("#port-extension").fill("50")
                    page.locator("#port-extension").press("Tab")
                    page.locator(".preferences-panel summary").click()
                result = page.evaluate("layoutProbe.running = false; layoutProbe.movement")
                assert page.evaluate("document.body.scrollWidth <= innerWidth"), f"Overflow at {width}px"
                if width >= 1100 and height >= 768:
                    footer = page.locator(".spectrum-readouts").bounding_box()
                    assert footer["y"] + footer["height"] <= height, (width, height, footer)
                    status = page.locator(".measurement-status").bounding_box()
                    assert status["y"] + status["height"] <= height, (width, height, status)
                    assert page.locator("#magnitude-plot").bounding_box()["y"] <= 120, "Keep the desktop plot close to the controls"
                    assert page.locator(".measurement-toolbar").bounding_box()["height"] <= 34, "Desktop acquisition controls fit one row"
                if result:
                    failures.append((width, result))
                print(f"{BOARD} {width}×{height}: {result or 'stable'}", flush=True)
                page.close()
        finally:
            browser.close()
            server.shutdown()
    if "--report" not in sys.argv:
        assert not failures, failures


if __name__ == "__main__":
    run()
