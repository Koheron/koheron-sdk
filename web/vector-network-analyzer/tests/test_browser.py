"""Real Chromium checks. Run in the SDK's Python/Playwright development image."""
from functools import partial
from http.server import SimpleHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from threading import Thread
import json
import math
import sys
from urllib.request import urlopen
from playwright.sync_api import sync_playwright

ROOT = Path(__file__).resolve().parents[3]
BOARD = "red-pitaya" if "--red-pitaya" in sys.argv else "alpha250"
OUTPUT = ROOT / f"tmp/examples/{BOARD}/vector-network-analyzer"
WEB = OUTPUT / "web"

class QuietHandler(SimpleHTTPRequestHandler):
    def log_message(self, *args):
        pass

def run():
    server = ThreadingHTTPServer(("127.0.0.1", 0), partial(QuietHandler, directory=str(WEB)))
    Thread(target=server.serve_forever, daemon=True).start()
    url = f"http://127.0.0.1:{server.server_port}/"
    screenshots = OUTPUT / "screenshots"
    screenshots.mkdir(exist_ok=True)
    with sync_playwright() as p:
        browser = p.chromium.launch(headless=True, args=["--no-sandbox"])
        try:
            page = browser.new_page(viewport={"width":1440,"height":1000}, device_scale_factor=1)
            errors=[]
            page.on("pageerror", lambda error: errors.append(str(error)))
            page.goto(url + "?demo=1")
            page.wait_for_function("window.vna && !vna.model.latest")
            assert page.locator("#connection-status").inner_text() == "Synthetic demo"
            if BOARD == "red-pitaya":
                assert page.locator("#board-name").inner_text() == "Red Pitaya"
                assert page.locator("#stop-frequency").input_value() == "50"
                assert "125 MS/s" in page.locator("#connection-detail").inner_text() or "Simulated" in page.locator("#connection-detail").inner_text()
                assert "OUT1" in page.locator("#empty-state").inner_text()
                assert "OUT2 muted" in page.locator("#board-notes").inner_text()
            page.screenshot(path=str(screenshots/"empty-desktop.png"), full_page=True)
            page.locator("#calibrate").click()
            assert page.locator("#calibration-dialog").is_visible()
            page.locator("#confirm-calibration").click()
            page.wait_for_function("vna.model.calibration && !document.querySelector('#run').disabled")
            page.locator("#run").click()
            page.wait_for_function("vna.model.latest && !document.querySelector('#run').disabled")
            assert page.locator("#correction-label").inner_text() == "Thru corrected"
            page.locator("#capture-reference").click()
            page.locator(".reference-row input[type=text]").fill("24 MHz low-pass")
            page.locator(".reference-row input[type=text]").press("Tab")
            page.locator("#find-cutoff").click()
            assert page.locator("#marker-1").input_value().endswith("MHz")
            # Shared Flot selection synchronizes both plots and preserves reset.
            box=page.locator("#magnitude-plot").bounding_box()
            page.mouse.move(box['x']+box['width']*.3,box['y']+box['height']*.5)
            page.mouse.down()
            page.mouse.move(box['x']+box['width']*.7,box['y']+box['height']*.5,steps=8)
            page.mouse.up()
            page.wait_for_function("vna.plot.plots[0].getAxes().xaxis.min > 1000")
            assert page.evaluate("vna.plot.plots[0].getAxes().xaxis.min === vna.plot.plots[1].getAxes().xaxis.min")
            assert page.locator("#marker-0").input_value() == "1 MHz"
            page.locator("#magnitude-plot").press("Escape")
            assert page.evaluate("vna.plot.plots.every(p=>p.getAxes().xaxis.min === 1000)")
            page.locator(".replace-reference").click()
            assert page.locator("#undo-reference").is_visible()
            page.locator("#undo-reference").click()
            assert page.locator(".reference-row input[type=text]").input_value() == "24 MHz low-pass"
            page.locator(".remove-reference").click()
            assert page.locator("#reference-count").inner_text() == "0 / 8"
            assert page.evaluate("document.activeElement.id === 'undo-reference'")
            page.locator("#undo-reference").click()
            assert page.locator("#reference-count").inner_text() == "1 / 8"
            with page.expect_download() as download:
                page.locator("#save-references").click()
            download.value.save_as(str(OUTPUT/"test-references.json"))
            assert json.loads((OUTPUT/"test-references.json").read_text())["format"] == "koheron-vna-references"
            # Names remain text in both the shared panel and Flot's HTML legend.
            page.locator(".reference-row input[type=text]").fill("<img src=x onerror=alert(1)>")
            page.locator(".reference-row input[type=text]").press("Tab")
            assert page.locator(".legend img").count() == 0
            page.locator(".reference-row input[type=text]").fill("24 MHz low-pass")
            page.locator(".reference-row input[type=text]").press("Tab")
            page.screenshot(path=str(screenshots/"measurement-desktop.png"), full_page=True)
            page.locator("#view-delay").click()
            assert page.locator("#secondary-title").inner_text() == "Group delay"
            page.locator("#view-phase").click()
            page.locator("#unwrap").check()
            page.locator("#marker-0").fill("banana")
            page.locator("#marker-0").press("Tab")
            assert page.locator("#marker-0").get_attribute("aria-invalid") == "true"
            page.locator("#marker-0").fill("1 MHz")
            page.locator("#marker-0").press("Tab")
            page.locator("#source-level").fill("-12")
            page.locator("#source-level").press("Tab")
            assert page.locator("#calibration-badge").inner_text() == "SETTINGS CHANGED"
            page.locator("#run").click()
            page.wait_for_function("vna.model.latest.settings.level === -12")
            assert page.locator("#correction-label").inner_text() == "Uncalibrated"
            page.locator("#source-level").fill("")
            page.locator("#source-level").press("Tab")
            old=page.evaluate("vna.model.latest.timestamp")
            page.locator("#run").click()
            assert page.evaluate("vna.model.latest.timestamp") == old
            page.locator("#source-level").fill("-12")
            page.locator("#source-level").press("Tab")
            page.locator("#repeat").check()
            page.locator("#run").click()
            assert page.locator("#message").inner_text().startswith("Synthetic demo.")
            page.locator("#stop").click()
            page.wait_for_function("!document.querySelector('#run').disabled")
            assert not page.locator("#repeat").is_checked()
            page.locator(".export-menu summary").click()
            with page.expect_download() as download:
                page.locator("#export-csv").click()
            download.value.save_as(str(OUTPUT/"test-measurement.csv"))
            assert "source: demo" in (OUTPUT/"test-measurement.csv").read_text()
            page.locator(".export-menu summary").click()
            with page.expect_download() as download:
                page.locator("#export-png").click()
            download.value.save_as(str(OUTPUT/"test-response.png"))
            assert (OUTPUT/"test-response.png").stat().st_size>10000
            page.reload()
            page.wait_for_function("vna.model.latest && vna.model.references.length === 1")
            for width in [768,390]:
                page.set_viewport_size({"width":width,"height":844})
                page.wait_for_timeout(100)
                assert page.evaluate("document.body.scrollWidth <= innerWidth"), f"Horizontal overflow at {width}px"
                page.screenshot(path=str(screenshots/f"measurement-{width}.png"),full_page=True)
            assert not errors, errors
            print("Chromium: calibration, shared Flot zoom, references/undo/recapture, safe legends, markers, cancellation, exports, persistence and responsive layouts passed")
            page.close()
            if "--protocol" in sys.argv:
                page=browser.new_page(viewport={"width":1440,"height":1000})
                page.on("pageerror", lambda error: errors.append(str(error)))
                page.goto(url)
                page.wait_for_function("document.querySelector('#connection-status').textContent === 'Connected'")
                rate = 125e6 if BOARD == "red-pitaya" else 250e6
                def acquire(cached):
                    old=page.evaluate("vna.model.latest?.timestamp")
                    page.locator("#run").click()
                    page.wait_for_function("old => vna.model.latest && vna.model.latest.timestamp !== old && !document.querySelector('#run').disabled",arg=old,timeout=90000)
                    frame=page.evaluate("vna.model.latest")
                    assert frame["source"]=="hardware"
                    for f,re,im in zip(frame["frequency"],frame["real"],frame["imag"]):
                        angle=2*math.pi*f*11/rate
                        assert math.hypot(re-.5*math.cos(angle),im+.5*math.sin(angle))<.001, (f,re,im)
                    timing=json.loads(page.locator("#sweep-timing").get_attribute("data-timing"))
                    assert timing["referenceCached"] is cached, timing
                    state=json.load(urlopen("http://127.0.0.1:8090/"))
                    assert state["owned"]==0 and state["blocks"]==frame["recordSamples"]*2/262144
                    assert state["batches"]==state["blocks"]/page.evaluate("VNA.protocol.batchBlocks") and state["releases"]>=1
                    print(f"Protocol {BOARD}: {frame["recordSamples"]/1048576:g} Mi samples, cached={cached}, {timing["total"]/1000:.2f} s",flush=True)
                    return frame
                frame=acquire(False)
                frame=acquire(True)
                page.locator("#point-count").select_option("2001")
                frame=acquire(True)
                # Older sessions can contain arbitrary valid point counts.
                # A missing custom grid rebuilds the cache, retaining presets.
                page.evaluate("document.querySelector('#point-count').add(new Option('599','599'))")
                page.locator("#point-count").select_option("599")
                frame=acquire(False)
                page.locator("#point-count").select_option("501")
                frame=acquire(True)
                page.locator("#point-count").select_option("2001")
                frame=acquire(True)
                page.locator("#sweep-duration").select_option("0.1")
                frame=acquire(False)
                assert frame["recordSamples"] == (16 if BOARD == "red-pitaya" else 32)*1048576
                page.locator("#source-level").fill("-12")
                page.locator("#source-level").press("Tab")
                frame=acquire(False)
                frame=acquire(True)
                page.locator("#start-frequency").fill("2000")
                page.locator("#stop-frequency").fill("5")
                page.locator("#stop-frequency").press("Tab")
                page.locator("#sweep-duration").select_option("0.02")
                frame=acquire(False)
                assert frame["recordSamples"] == (4 if BOARD == "red-pitaya" else 8)*1048576
                page.locator("#record-mode").select_option("full")
                frame=acquire(False)
                assert frame["recordSamples"]==67108864
                page.locator("#record-mode").select_option("auto")
                frame=acquire(False)
                frame=acquire(True)
                for fault, message in [(11,"record length"),(10,"Incomplete ADC")]:
                    old=frame["timestamp"]
                    urlopen(f"http://127.0.0.1:8090/?fault={fault}").close()
                    page.locator("#run").click()
                    page.wait_for_function("text => !document.querySelector('#run').disabled && document.querySelector('#message').textContent.includes(text)",arg=message,timeout=30000)
                    assert page.evaluate("vna.model.latest.timestamp")==old
                    assert json.load(urlopen("http://127.0.0.1:8090/"))["owned"]==0
                old=frame["timestamp"]
                urlopen("http://127.0.0.1:8090/?fault=3").close()
                page.locator("#run").click()
                page.wait_for_function("!document.querySelector('#run').disabled && document.querySelector('#message').textContent.includes('clipping')",timeout=30000)
                assert page.evaluate("vna.model.latest.timestamp")==old
                assert json.load(urlopen("http://127.0.0.1:8090/"))["owned"]==0
                urlopen("http://127.0.0.1:8090/?fault=9").close()
                page.locator("#run").click()
                page.wait_for_function("vna.token > 0",timeout=30000)
                page.locator("#stop").click()
                page.wait_for_function("!document.querySelector('#run').disabled",timeout=10000)
                assert json.load(urlopen("http://127.0.0.1:8090/"))["owned"]==0
                urlopen("http://127.0.0.1:8090/?fault=0").close()
                frame=acquire(True)
                urlopen("http://127.0.0.1:8090/?fault=13").close()
                old=frame["timestamp"]
                page.locator("#run").click()
                page.wait_for_function("document.querySelector('#progress-title').textContent === 'Downloading record'",timeout=30000)
                # Repeated Stop clicks must not leave an old fallback timer
                # that can terminate the next sweep's persistent worker.
                page.evaluate("document.querySelector('#stop').click(); document.querySelector('#stop').click()")
                page.wait_for_function("!document.querySelector('#run').disabled",timeout=10000)
                state=json.load(urlopen("http://127.0.0.1:8090/"))
                assert state["owned"] == state["pending"] == state["releasePending"] == 0, state
                assert page.evaluate("vna.model.latest.timestamp") == old
                urlopen("http://127.0.0.1:8090/?fault=0").close()
                frame=acquire(True)
                page.evaluate("window.workerAfterRestart = vna.worker")
                page.wait_for_timeout(2600)
                assert page.evaluate("vna.worker !== null && vna.worker === workerAfterRestart")
                assert page.locator("#progress-title").inner_text() == "Sweep complete"
                assert page.evaluate("vna.model.latest.timestamp") == frame["timestamp"]
                # Cancel an unfinished reference transform; it must not enter the cache.
                page.locator("#record-mode").select_option("full")
                page.locator("#source-level").fill("-18")
                page.locator("#source-level").press("Tab")
                old=frame["timestamp"]
                page.locator("#run").click()
                page.wait_for_function("document.querySelector('#progress-title').textContent === 'Transforming reference'",timeout=90000)
                page.locator("#stop").click()
                page.wait_for_function("!document.querySelector('#run').disabled",timeout=10000)
                assert page.evaluate("vna.model.latest.timestamp")==old
                frame=acquire(False)
                frame=acquire(True)
                page.locator('#repeat').check()
                page.locator('#run').click()
                for _ in range(2):
                    old=frame['timestamp']
                    page.wait_for_function("old => vna.model.latest.timestamp !== old",arg=old,timeout=90000)
                    frame=page.evaluate('vna.model.latest')
                page.locator('#repeat').uncheck()
                page.wait_for_function("!document.querySelector('#run').disabled",timeout=90000)
                assert json.loads(page.locator('#sweep-timing').get_attribute('data-timing'))['referenceCached'] is True
                assert json.load(urlopen('http://127.0.0.1:8090/'))['owned']==0
                assert not errors,errors
                print("Chromium protocol: adaptive/full records, batched download, cached/changed references, known gain/delay, malformed records, clipping and drained cancellation/reuse passed")
                page.close()
        finally:
            browser.close()
    server.shutdown()

if __name__ == "__main__":
    run()
