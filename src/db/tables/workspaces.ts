/*
Workspace rows and in-memory CRUD operations.
*/

/** One workspace row from the workspaces store. */
export interface Workspace {
  id: string;
  name: string;
}

/** In-memory workspace rows and CRUD queries. */
export class WorkspacesTable {
  private readonly items = new Map<string, Workspace>();

  /** All workspaces in insertion order. */
  list(): Workspace[] {
    return Array.from(this.items.values());
  }

  /** Lookup workspace by id, or undefined when missing. */
  get(id: string): Workspace | undefined {
    return this.items.get(id);
  }

  /** Create a workspace with a new id. */
  create(name: string): Workspace {
    const id = crypto.randomUUID();
    const workspace: Workspace = { id, name };
    this.items.set(id, workspace);
    return workspace;
  }

  /** Replace workspace name when id exists; otherwise undefined. */
  replace(id: string, name: string): Workspace | undefined {
    if (!this.items.has(id)) {
      return undefined;
    }
    const updated: Workspace = { id, name };
    this.items.set(id, updated);
    return updated;
  }

  /** Patch workspace name when id exists; otherwise undefined. */
  patch(id: string, name: string): Workspace | undefined {
    const existing = this.items.get(id);
    if (!existing) {
      return undefined;
    }
    const updated: Workspace = { ...existing, name };
    this.items.set(id, updated);
    return updated;
  }

  /** Delete workspace by id; true if deleted, false if not found. */
  delete(id: string): boolean {
    return this.items.delete(id);
  }
}
