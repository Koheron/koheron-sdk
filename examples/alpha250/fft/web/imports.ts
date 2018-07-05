class Imports {
  private importLinks: HTMLLinkElement[];

  constructor (document: Document) {
    this.importLinks = <HTMLLinkElement[]><any>document.querySelectorAll('link[rel="import"]');
    for (let i = 0; i < this.importLinks.length ; i++) {
      let template = <any>this.importLinks[i].import.querySelector('.template');
      let clone = document.importNode(template.content, true);
      let parentId = this.importLinks[i].dataset.parent;
      document.getElementById(parentId).appendChild(clone);
    }
  }
}
