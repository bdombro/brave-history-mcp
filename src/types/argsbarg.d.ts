import type { AppDb } from "../db/index.ts";

declare module "argsbarg" {
  interface Locals {
    db: AppDb;
  }

  interface ServerState {
    db?: AppDb;
  }
}
