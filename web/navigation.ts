class Navigation {
    private navigationDiv: HTMLDivElement;
    private navigationLinksDiv: HTMLDivElement;
    private collapseBtn: HTMLButtonElement;
    private mainDiv: HTMLDivElement;

    constructor(document: Document) {
        this.navigationDiv = <HTMLDivElement>document.getElementById("navigation");
        this.navigationLinksDiv = <HTMLDivElement>document.getElementById("navigation-links");
        this.collapseBtn = <HTMLButtonElement>document.getElementById("collapse-btn");
        this.mainDiv = <HTMLDivElement>document.getElementById("main");
    }

    openNavigation(): void {
        this.navigationDiv.style.width = "100px";
        this.mainDiv.style.marginLeft = "100px";
        this.collapseBtn.innerHTML = "&#60;";
        this.collapseBtn.value = "close";
        this.navigationLinksDiv.style.display = "block";
    }

    closeNavigation(): void {
        this.navigationDiv.style.width = "30px";
        this.mainDiv.style.marginLeft = "30px";
        this.collapseBtn.innerHTML = "&#62;";
        this.collapseBtn.value = "open";
        this.navigationLinksDiv.style.display = "none";
    }

    collapseBtnClick(event): void {
        if (event.value === 'close') {
            this.closeNavigation();
        } else if (event.value === 'open') {
            this.openNavigation();
        }
    }

}