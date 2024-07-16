class App {

  private imports: Imports;
  private control: Control;
  private dpll: Dpll;
  private clockGenerator: ClockGenerator;
  private clockGeneratorApp: ClockGeneratorApp;

  constructor(window: Window, document: Document, ip: string) {
    let sockpoolSize: number = 10;
    let client = new Client(ip, sockpoolSize);

    window.addEventListener('HTMLImportsLoaded', () => {
      client.init( () => {
        this.imports = new Imports(document);
        this.dpll = new Dpll(client);
        this.clockGenerator = new ClockGenerator(client);
        this.clockGeneratorApp = new ClockGeneratorApp(document, this.clockGenerator);
        this.control = new Control(document, this.dpll);
      });
    }, false);

    window.onbeforeunload = () => { client.exit(); };
    }
}

let app = new App(window, document, location.hostname);