export type ModrinthProjectType = 'mod' | 'modpack' | 'datapack' | 'resourcepack' | 'plugin';
export interface ModrinthSearchResult {
    project_id: string;
    project_type: ModrinthProjectType;
    slug: string;
    author: string;
    title: string;
    description: string;
    categories: string[];
    display_categories: string[];
    versions: string[];
    downloads: number;
    follows: number;
    icon_url: string;
    date_created: string;
    date_modified: string;
    latest_version: string;
    license: string;
}
export interface ModrinthVersionFile {
    hashes: {
        sha1?: string;
        sha512?: string;
    };
    url: string;
    filename: string;
    primary: boolean;
    size: number;
}
export interface ModrinthVersionDependency {
    version_id?: string;
    project_id?: string;
    file_name?: string;
    dependency_type: 'required' | 'optional' | 'incompatible' | 'embedded';
}
export interface ModrinthProjectVersion {
    id: string;
    project_id: string;
    author_id: string;
    name: string;
    version_number: string;
    game_versions: string[];
    loaders: string[];
    featured: boolean;
    files: ModrinthVersionFile[];
    dependencies: ModrinthVersionDependency[];
    date_published: string;
    downloads: number;
}
export interface SearchOptions {
    query?: string;
    facets?: string[][];
    index?: 'relevance' | 'downloads' | 'follows' | 'newest' | 'updated';
    offset?: number;
    limit?: number;
}
export declare class ModrinthClient {
    private baseUrl;
    private userAgent;
    private searchCache;
    private cacheTtlMs;
    search(options?: SearchOptions): Promise<{
        hits: ModrinthSearchResult[];
        offset: number;
        limit: number;
        total_hits: number;
    }>;
    getProjectVersions(projectIdOrSlug: string, loaders?: string[], gameVersions?: string[]): Promise<ModrinthProjectVersion[]>;
    getProject(projectIdOrSlug: string): Promise<any>;
    /**
     * Installs a mod or mod version into the target directory.
     */
    installVersion(version: ModrinthProjectVersion, targetDir: string, onProgress?: (bytes: number, total: number) => void): Promise<{
        fileName: string;
        filePath: string;
    }>;
    private downloadFile;
    private getJson;
}
