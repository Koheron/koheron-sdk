// Give passive monitor traffic its own ordered socket so spectra cannot queue
// ahead of gain or reference edits on the feedback-control connection.
class DpllMonitor extends PnaMonitor {
  constructor(document: Document, ip: string, fail: (error: unknown) => void) {
    super(document, new Client(ip, 1), 'Dma', fail);
  }
}
