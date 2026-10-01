// (c) Koheron
// KoheronSystem: fetch system metadata

interface ManifestData {
    release: string;
    build_id: string;
    project: string;
    board: string;
    zynq: string;
    kernel: string;
    u_boot: string;
    device_tree: string;
    koheron_version: string;
    git_commit: string;
    git_branch: string;
    git_tag: string;
    git_describe: string;
    git_dirty: string;
    generated_utc: string;
    [key: string]: string; // allow extra keys
}

interface ReleaseData {
    NAME: string;
    RELEASE: string;
    BUILD_ID: string;
    UBUNTU: string;
    KOHERON_VERSION: string;
    GIT_COMMIT: string;
    GIT_BRANCH: string;
    GIT_TAG: string;
    GIT_DESCRIBE: string;
    GIT_DIRTY: string;
    [key: string]: string;
}

interface BuildSummary {
    manifest: ManifestData;
    release: ReleaseData;
}

type ReleasePlus = ReleaseData & {
    kernel?: string;
    zynq?: string;
    generated_utc?: string;
  };

/**
 * Client for Koheron system manifest/release endpoints.
 */
class KoheronSystem {
    private readonly baseUrl: string;
  
    constructor(baseUrl: string = "") {
        // baseUrl can be set to "" for same-origin requests
        this.baseUrl = baseUrl.replace(/\/$/, "");
    }
  
    /** Fetch manifest (key/value data) */
    async getManifest(): Promise<ManifestData> {
        return this.fetchJson<ManifestData>(`${this.baseUrl}/api/system/manifest`);
    }
  
    /** Fetch release (/etc/koheron-release) data */
    async getRelease(): Promise<ReleaseData> {
        return this.fetchJson<ReleaseData>(`${this.baseUrl}/api/system/release`);
    }
  
    /** Fetch combined manifest + release */
    async getBuildSummary(): Promise<BuildSummary> {
        return this.fetchJson<BuildSummary>(`${this.baseUrl}/api/system/build`);
    }

    /**
     * Fetch build summary and return a merged object:
     * release fields + { kernel, zynq, generated_utc } from manifest.
     */
    async getReleasePlus(): Promise<ReleasePlus> {
        const s = await this.getBuildSummary();
        const out: ReleasePlus = { ...s.release };
        if (s.manifest.kernel)        out.kernel = s.manifest.kernel;
        if (s.manifest.zynq)          out.zynq = s.manifest.zynq;
        if (s.manifest.generated_utc) out.generated_utc = s.manifest.generated_utc;
        return out;
    }

      // ----------- Metadata for UI -----------
    /** Recommended field ordering for manifest table */
    static readonly manifestOrder: ReadonlyArray<keyof ManifestData> = [
        "release","build_id","project","board","zynq","kernel","u_boot","device_tree",
        "koheron_version","git_commit","git_branch","git_tag","git_describe","git_dirty","generated_utc"
    ];

    /** Friendly display names for manifest keys */
    static readonly manifestLabels: Record<string, string> = {
        release: "Release",
        build_id: "Build ID",
        project: "Project",
        board: "Board",
        zynq: "Zynq",
        kernel: "Kernel",
        u_boot: "U-Boot",
        device_tree: "Device Tree",
        koheron_version: "Koheron",
        git_commit: "Git Commit",
        git_branch: "Git Branch",
        git_tag: "Git Tag",
        git_describe: "Git Describe",
        git_dirty: "Git Dirty",
        generated_utc: "Generated (UTC)"
    };

    /** Recommended field ordering for release table */
    static readonly releaseOrder: ReadonlyArray<keyof ReleaseData> = [
        "NAME","RELEASE","BUILD_ID","UBUNTU","KOHERON_VERSION",
        "GIT_COMMIT","GIT_BRANCH","GIT_TAG","GIT_DESCRIBE","GIT_DIRTY"
    ];

    /** Preferred display order: release fields + selected manifest fields */
    static readonly releaseDisplayOrder: ReadonlyArray<string> = [
        "NAME","RELEASE","BUILD_ID","UBUNTU","KOHERON_VERSION",
        "GIT_COMMIT","GIT_BRANCH","GIT_TAG","GIT_DESCRIBE","GIT_DIRTY",
        // appended from manifest:
        "kernel","zynq","generated_utc"
    ];

      /** Labels for display (add the three appended keys) */
    static readonly releaseDisplayLabels: Record<string, string> = {
        NAME: "Name",
        RELEASE: "Release",
        BUILD_ID: "Build ID",
        UBUNTU: "Ubuntu",
        KOHERON_VERSION: "Koheron",
        GIT_COMMIT: "Git Commit",
        GIT_BRANCH: "Git Branch",
        GIT_TAG: "Git Tag",
        GIT_DESCRIBE: "Git Describe",
        GIT_DIRTY: "Git Dirty",
        kernel: "Kernel",
        zynq: "Zynq",
        generated_utc: "Generated (UTC)"
    };

    // ----------- internals -----------
  
    /** Low-level GET helper with proper error handling */
    private async fetchJson<T>(url: string): Promise<T> {
        const resp = await fetch(url, { method: "GET" });
        if (!resp.ok) {
            throw new Error(`Failed to fetch ${url} (${resp.status})`);
        }
        return resp.json() as Promise<T>;
    }
}
  