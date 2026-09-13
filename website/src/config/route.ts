import { paths } from "./paths"

export const route = {
  ROOT: paths.root,
  HOME: paths.home,
  WHY: paths.why,
  BENCHMARKS: paths.benchmarks,
  FEATURES: paths.features,
  INTEGRATIONS: paths.integrations,
  QUICKSTART: paths.quickstart,
  DOCS: paths.docs,
  ...paths,
} as const

export const routes = route
export default route