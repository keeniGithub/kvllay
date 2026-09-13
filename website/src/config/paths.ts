const base = (import.meta.env.BASE_URL || "/").endsWith("/")
  ? (import.meta.env.BASE_URL || "/")
  : `${import.meta.env.BASE_URL}/`

export const paths = {
  root: base,
  home: "#",
  why: "#why",
  benchmarks: "#benchmarks",
  features: "#features",
  integrations: "#integrations",
  quickstart: "#quickstart",
  docs: "#docs",
  assets: {
    allayHeader: `${base}allay_header.webp`,
    allayFly: `${base}allay_fly.webp`,
    allayDocsWebp: `${base}allay_docs.webp`,
    allayDocsGif: `${base}allay_docs.gif`,
  },
  sections: {
    why: "why",
    benchmarks: "benchmarks",
    features: "features",
    integrations: "integrations",
    quickstart: "quickstart",
    docs: "docs",
  },
} as const

export default paths
