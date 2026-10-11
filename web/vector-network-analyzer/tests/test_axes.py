"""Y-axis stability and refitting through the production Flot renderer."""
from functools import partial
from http.server import ThreadingHTTPServer
from threading import Thread
from playwright.sync_api import sync_playwright
from test_browser import BOARD, WEB, QuietHandler


def run():
    server = ThreadingHTTPServer(("127.0.0.1", 0), partial(QuietHandler, directory=str(WEB)))
    Thread(target=server.serve_forever, daemon=True).start()
    with sync_playwright() as p:
        browser = p.chromium.launch(headless=True, args=["--no-sandbox"])
        try:
            page = browser.new_page(viewport={"width": 1360, "height": 768})
            errors = []
            page.on("pageerror", lambda error: errors.append(str(error)))
            page.goto(f"http://127.0.0.1:{server.server_port}/?demo=1")
            page.wait_for_function("window.vna && !document.querySelector('#run').disabled")
            page.locator("#unwrap").check()
            page.evaluate("""() => {
                window.axisState = () => vna.plot.plots.map(plot => {
                    const axis = plot.getAxes().yaxis;
                    return [axis.min, axis.max, axis.ticks.map(t => [t.v, t.label, axis.p2c(t.v)])];
                });
                window.axisFrame = (shift = 0, delay = 40, missing = false) => {
                    const settings = {start: 1000, stop: 1e6, points: 101, duration: .1, level: -30, record: 'auto'};
                    const frequency = Array.from({length: 101}, (_, i) => 1000 * 1000 ** (i / 100));
                    const amplitude = frequency.map((_, i) => 10 ** ((-20 + i / 5 + shift) / 20));
                    return {source: 'demo', settings, frequency, timestamp: new Date().toISOString(),
                        recordSamples: VNA.VNAModel.recordSamples(settings),
                        real: frequency.map((f, i) => missing ? null : amplitude[i] * Math.cos(-2 * Math.PI * f * delay * 1e-9)),
                        imag: frequency.map((f, i) => missing ? null : amplitude[i] * Math.sin(-2 * Math.PI * f * delay * 1e-9))};
                };
                window.drawAxisFrame = (shift = 0, delay = 40, missing = false) => {
                    vna.model.latest = axisFrame(shift, delay, missing);
                    vna.model.settings = {...vna.model.latest.settings};
                    vna.plot.draw();
                    return axisState();
                };
                window.axisCycles = 0;
                window.repeatAxes = [];
                window.gridRebuilds = [0, 0];
                vna.plot.plots.forEach((plot, panel) => {
                    const update = plot.updateRanges.bind(plot);
                    plot.updateRanges = (...args) => { gridRebuilds[panel]++; return update(...args); };
                });
                vna.demoFrame = () => {
                    // Small changes in both measured gain and phase each cycle.
                    const i = axisCycles++;
                    const frame = axisFrame([0, .2, -.2, .1][i % 4], [40, 40.1, 39.9, 40.2][i % 4]);
                    // Object key order carries no measurement meaning.
                    if (i % 2) frame.settings = Object.fromEntries(Object.entries(frame.settings).reverse());
                    return frame;
                };
                const accept = vna.accept.bind(vna);
                vna.accept = frame => {
                    accept(frame);
                    if (!repeatAxes.length) gridRebuilds = [0, 0];
                    repeatAxes.push(axisState());
                };
            }""")
            # Actual Repeat acquisition/rendering: limits, ticks and grid pixels stay exact.
            page.locator("#repeat").check()
            page.locator("#run").click()
            page.wait_for_function("repeatAxes.length >= 8")
            page.locator("#repeat").uncheck()
            page.wait_for_function("!document.querySelector('#run').disabled")
            snapshots = page.evaluate("repeatAxes")
            assert all(axes == snapshots[0] for axes in snapshots), snapshots
            assert page.evaluate("gridRebuilds") == [0, 0], "Stable sweeps update data without rebuilding grids"
            baseline = page.evaluate("drawAxisFrame(0, 40)")
            # The padded candidate can exceed the old range even when data fits.
            assert page.evaluate("drawAxisFrame(2.8, 40.1)") == baseline
            assert page.evaluate("drawAxisFrame(0, 40, true)") == baseline
            upper = page.evaluate("drawAxisFrame(10, 40)")
            assert upper[0][0] == baseline[0][0] and upper[0][1] > baseline[0][1]
            assert upper[1] == baseline[1]
            lower = page.evaluate("drawAxisFrame(-10, 40)")
            assert lower[0][0] < upper[0][0] and lower[0][1] == upper[0][1]
            assert page.evaluate("drawAxisFrame(0, 40)") == lower
            page.locator("#reset-view").click()
            assert page.evaluate("axisState()") == baseline
            # Wrapped phase is fixed; returning to unwrapped phase fits anew.
            page.locator("#unwrap").uncheck()
            wrapped = page.evaluate("drawAxisFrame(0, 80)")
            assert wrapped[1][:2] == [-180, 180]
            page.locator("#unwrap").check()
            unwrapped = page.evaluate("axisState()")
            assert unwrapped[1][0] < baseline[1][0]
            assert page.evaluate("drawAxisFrame(.1, 80.1)") == unwrapped
            page.locator("#view-delay").click()
            delay = page.evaluate("axisState()")
            assert delay[1][0] < 80 < delay[1][1]
            assert page.evaluate("drawAxisFrame(.2, 80.2)") == delay
            # An explicit extension changes the displayed delay and refits its axis.
            page.locator(".preferences-panel summary").click()
            delay = page.evaluate("axisState()")
            page.locator("#port-extension").fill("50")
            page.locator("#port-extension").press("Tab")
            extended = page.evaluate("axisState()")
            assert extended[1][0] < 30.2 < extended[1][1] < delay[1][0]
            assert extended[0][:2] == delay[0][:2], (extended[0], delay[0])
            page.locator("#port-extension").fill("0")
            page.locator("#port-extension").press("Tab")
            # References influence fitting; hiding one releases its old extrema.
            page.evaluate("drawAxisFrame(50, 80)")
            page.locator("#capture-reference").click()
            page.evaluate("drawAxisFrame(0, 80)")
            with_reference = page.evaluate("axisState()")
            page.locator(".reference-row input[type=checkbox]").uncheck()
            without_reference = page.evaluate("axisState()")
            assert without_reference[0][1] < with_reference[0][1]
            assert page.evaluate("drawAxisFrame(.2, 80.2)") == without_reference
            # X selection refits only the visible data and remains stable afterward.
            page.locator("#view-phase").click()
            box = page.locator("#magnitude-plot").bounding_box()
            page.mouse.move(box["x"] + box["width"] * .35, box["y"] + box["height"] * .5)
            page.mouse.down()
            page.mouse.move(box["x"] + box["width"] * .65, box["y"] + box["height"] * .5, steps=8)
            page.mouse.up()
            zoom = page.evaluate("axisState()")
            assert zoom[0][1] - zoom[0][0] < without_reference[0][1] - without_reference[0][0]
            assert page.evaluate("drawAxisFrame(.1, 80.1)") == zoom
            # A frame arriving mid-drag leaves BOTH plots on the displayed frame.
            page.locator("#reset-view").click()
            page.evaluate("drawAxisFrame(0, 40)")
            old_series = page.evaluate("vna.plot.plots.map(p => p.getData().map(s => s.data))")
            box = page.locator("#magnitude-plot").bounding_box()
            x, y = box["x"] + box["width"] * .3, box["y"] + box["height"] * .5
            page.mouse.move(x, y)
            page.mouse.down()
            page.mouse.move(box["x"] + box["width"] * .7, y, steps=8)
            page.evaluate("drawAxisFrame(12, 140)")
            assert page.evaluate("vna.plot.plots.map(p => p.getData().map(s => s.data))") == old_series
            page.mouse.up()
            assert page.evaluate("vna.plot.plots.every(p => p.getAxes().xaxis.min === vna.plot.plots[0].getAxes().xaxis.min)")
            assert page.evaluate("vna.plot.current.frame.timestamp === vna.model.latest.timestamp")
            assert page.evaluate("vna.plot.plots[0].getData()[0].data[50][1] > 0")
            # An unfinished/too-short selection also resumes the pending frame.
            page.mouse.move(x, y)
            page.mouse.down()
            page.evaluate("drawAxisFrame(-12, 250)")
            page.mouse.up()
            assert page.evaluate("vna.plot.current.frame.timestamp === vna.model.latest.timestamp")
            assert page.evaluate("vna.plot.plots[0].getData()[0].data[50][1] < -20")
            page.locator("#reset-view").click()
            # A stationary hover follows new results and the selected view's unit.
            page.mouse.move(box["x"] + box["width"] * .6, y)
            old_hover = page.locator("#hover-reading").inner_text()
            page.evaluate("drawAxisFrame(-10, 200)")
            assert page.locator("#hover-reading").inner_text() != old_hover
            page.evaluate("vna.plot.delayView = true; vna.plot.draw()")
            assert "200.00 ns" in page.locator("#hover-reading").inner_text()
            page.mouse.move(5, 5)
            assert page.locator("#hover-reading").inner_text() == "Drag to zoom · double-click to reset"
            assert not errors, errors
            print(f"{BOARD}: repeat Y limits/ticks stable without grid rebuilds; expansion/refitting, synchronized drag/release and live hover passed", flush=True)
        finally:
            browser.close()
            server.shutdown()


if __name__ == "__main__":
    run()
