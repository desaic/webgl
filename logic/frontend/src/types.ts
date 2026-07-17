// Additional types for domain-aware graph data from the backend
export interface Node {
  id: number;
  label: string;
  kind: string;       // FACT | COMPOUND | PORTAL | E_CLASS | GOAL |
                       // CONTRADICTION | RESOURCE | TOOL_RESULT | AGENT_STATE
  domain?: string;    // math, physics_rigid, grammar, survival, etc.
  detail?: string;    // full formula or resource path
  context?: number[]; // compound node ids this belongs to
}

export interface Edge {
  from: number | number[];  // array for hyper-edges
  to: number;
  relation: string;   // RULE | HYPER | INSTANTIATE | REWRITE | TOOL | AUDIT | DOMAIN
  rule?: string;      // rule name for RULE edges
  label?: string;
}

export interface ProofGraph {
  goal?: string;
  domains_used?: string[];
  nodes: Node[];
  edges: Edge[];
  audit_trail?: AuditStep[];
}

export interface AuditStep {
  step: number;
  created: number;    // node id
  by: string;         // rule name
  from: number[];     // premise node ids
  label?: string;
}
