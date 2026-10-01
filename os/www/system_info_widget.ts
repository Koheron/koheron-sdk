class SystemInfoWidget {
    private releaseTable: HTMLTableElement;

    constructor (document: Document) {
        // Table collapse
        const header = <HTMLElement>document.getElementById("system-info-header");
        const content = <HTMLElement>document.getElementById("system-info-content");
        const arrow = <HTMLElement>document.getElementById("system-info-arrow");
  
        header.addEventListener("click", () => {
          const isCollapsed = content.classList.toggle("collapsed");
          arrow.textContent = isCollapsed ? "▶" : "▼";
        });
  
        content.classList.add("collapsed");
        arrow.textContent = "▶";

        // Fill table
        this.releaseTable = <HTMLTableElement>document.getElementById("release-table");
        const sys = new KoheronSystem();
        (async () => {
            const relPlus = await sys.getReleasePlus();
            this.renderTable(
                relPlus,
                KoheronSystem.releaseDisplayOrder,
                KoheronSystem.releaseDisplayLabels
          );
        })();
    }

    renderTable(
        data: Record<string, string>,
        order: readonly string[],
        labels: Record<string, string>
    ) {
        this.releaseTable.innerHTML = "";
        const tbody = document.createElement("tbody");

        for (const key of order) {
            if (!(key in data)) {
                continue;
            }

            const tr = document.createElement("tr");
            const label = labels[key] ?? key;
            let val = data[key];

            if (key === "generated_utc" && val) {
                try { val = new Date(val).toLocaleString(); } catch {}
            }

            tr.innerHTML = `<td>${label}</td><td>${val ?? ""}</td>`;
            tbody.appendChild(tr);
        }

        this.releaseTable.appendChild(tbody);
    }
}