import { useState } from "react"
import { IconDatabase, IconGauge, IconArrowsExchange, IconChevronLeft, IconChevronRight } from "@tabler/icons-react"
import benchmarkSingleClient from "@docs/images/benchmark_single_client.png"
import benchmarkMultithreaded from "@docs/images/benchmark_multithreaded.png"
import benchmarkRam from "@docs/images/benchmark_ram.png"
import benchmarkDocker from "@docs/images/benchmark_docker.png"
import benchmarkLatency from "@docs/images/benchmark_latency.png"
import { paths } from "@/config/paths"
import { Reveal } from "@/components/ui/Reveal"

export function Comparison() {
  const [activeSlide, setActiveSlide] = useState(0)

  const slides = [
    {
      id: "single",
      tabName: "Single Client",
      title: "Single-Client Throughput (SET, GET, INCR, MSET)",
      subtitle: "Single socket connection throughput — 84.7k ops/s INCR (+20.3%) and 78.7k ops/s MSET (+33.0% over Redis).",
      badge: "+20.3% Faster INCR",
      image: benchmarkSingleClient,
      stats: "Kvllay: 84.7k INCR / 78.7k MSET • Redis 8.x: 70.4k INCR / 59.1k MSET",
    },
    {
      id: "multi",
      tabName: "Multi-Threaded",
      title: "Multithreaded Workload (Parallel & Concurrent)",
      subtitle: "Parallel worker threads & high-concurrency client pools — 139.9k ops/s INCR and 136.8k ops/s GET (outperforming Redis).",
      badge: "139.9k ops/s Peak",
      image: benchmarkMultithreaded,
      stats: "Kvllay: 139,860 ops/s • Redis 8.x: 136,799 ops/s",
    },
    {
      id: "ram",
      tabName: "RAM Usage",
      title: "RAM Consumption (Idle & 50k Keys)",
      subtitle: "Resident set size (RSS) memory consumption measured via OS procfs under clean and populated states.",
      badge: "3.7x Lighter",
      image: benchmarkRam,
      stats: "Kvllay: 4.1 MB Idle (11.4 MB @ 50k keys) • Redis 8.x: 15.2 MB Idle (20.0 MB @ 50k keys)",
    },
    {
      id: "docker",
      tabName: "Docker Size",
      title: "Docker Scratch Container Footprint",
      subtitle: "Multi-stage scratch image size compared to official redis:alpine image.",
      badge: "87x Smaller",
      image: benchmarkDocker,
      stats: "Kvllay Container: 1.6 MB • Redis Image: 140.0 MB",
    },
    {
      id: "latency",
      tabName: "Latency p50",
      title: "Sub-Millisecond Response Latency (p50)",
      subtitle: "50th percentile response time distribution across concurrent benchmark requests — kvllay achieves down to 47 μs (10% lower latency than Redis).",
      badge: "47 μs Latency",
      image: benchmarkLatency,
      stats: "Kvllay: 0.047 ms (47 μs) • Redis 8.x: 0.052 ms (52 μs)",
    },
  ]

  const prevSlide = () => {
    setActiveSlide((prev) => (prev === 0 ? slides.length - 1 : prev - 1))
  }

  const nextSlide = () => {
    setActiveSlide((prev) => (prev === slides.length - 1 ? 0 : prev + 1))
  }

  const highlights = [
    {
      icon: IconDatabase,
      title: "Ultra-Lean Footprint",
      desc: "At just 1.6 MB in Docker scratch and 4.1 MB idle RAM, Kvllay fits where Redis won't: CI runners, local microservices, lightweight containers, and edge IoT devices.",
      badgeHighlight: "87x Smaller",
      badgeText: "than standard Redis image",
    },
    {
      icon: IconGauge,
      title: "Sub-Millisecond & Fast",
      desc: "Guaranteed p50 latency down to 0.047 ms (47 μs) on concurrent workloads, beating Redis across both single-client and multi-threaded throughput with zero memory bloat.",
      badgeHighlight: "47 μs p50",
      badgeText: "response latency",
    },
    {
      icon: IconArrowsExchange,
      title: "Zero Code Migration",
      desc: "Pure RESP2 compatibility means existing python-redis, node-redis, predis, Jedis, and redis-cli connect out of the box — just swap the port or host.",
      badgeHighlight: "100% Drop-in",
      badgeText: "standard primary protocol",
    },
  ]

  const tableData = [
    {
      metric: "Single-Client SET Throughput",
      kvllay: "78,077 ops/s",
      redis: "69,913 ops/s",
      result: "+11.7% Faster than Redis",
    },
    {
      metric: "Single-Client GET Throughput",
      kvllay: "82,471 ops/s",
      redis: "70,437 ops/s",
      result: "+17.1% Faster than Redis",
    },
    {
      metric: "Atomic Counter (INCR)",
      kvllay: "84,718 ops/s",
      redis: "70,422 ops/s",
      result: "+20.3% Faster than Redis",
    },
    {
      metric: "Batch Multi-Set (MSET 5 keys)",
      kvllay: "78,665 ops/s",
      redis: "59,143 ops/s",
      result: "+33.0% Faster than Redis",
    },
    {
      metric: "Concurrent Counter (50 clients) INCR",
      kvllay: "139,860 ops/s",
      redis: "136,799 ops/s",
      result: "+2.2% Faster than Redis",
    },
    {
      metric: "Concurrent Read (50 clients) GET",
      kvllay: "136,799 ops/s",
      redis: "131,752 ops/s",
      result: "+3.8% Faster than Redis",
    },
    {
      metric: "Parallel Write (8 threads) SET",
      kvllay: "120,268 ops/s",
      redis: "119,629 ops/s",
      result: "kvllay ahead with 32-shard striping",
    },
    {
      metric: "Response Latency p50 (Parallel)",
      kvllay: "0.047 ms (47 μs)",
      redis: "0.052 ms (52 μs)",
      result: "10% lower latency than Redis",
    },
    {
      metric: "Idle Footprint (RAM)",
      kvllay: "4.1 MB",
      redis: "15.2 MB",
      result: "3.7x lighter on server memory",
    },
    {
      metric: "RAM with 50,000 Keys",
      kvllay: "11.4 MB",
      redis: "20.0 MB",
      result: "43% less RAM consumption",
    },
    {
      metric: "Docker Container Size",
      kvllay: "1.6 MB (Scratch)",
      redis: "~140 MB",
      result: "87x smaller download",
    },
    {
      metric: "Cold Start Execution Time",
      kvllay: "3.25 ms",
      redis: "7.65 ms",
      result: "2.4x faster cold start",
    },
    {
      metric: "Persistence (AOF everysec) SET",
      kvllay: "103,386 ops/s",
      redis: "113,286 ops/s",
      result: ">100k ops/s with durable log",
    },
  ]

  return (
    <section id={paths.sections.why} className="w-full flex flex-col items-center py-10 sm:py-16 px-4 sm:px-6 lg:px-10 max-w-[1240px] mx-auto scroll-mt-20">
      <Reveal direction="up" className="w-full flex flex-col items-center text-center gap-2.5 sm:gap-3 mb-8 sm:mb-12">
        <div className="inline-flex items-center gap-2 px-3 py-1 rounded-full bg-[#00D2FF]/10 border border-[#00D2FF]/30">
          <span className="text-[11px] font-mono font-bold text-[#00D2FF] tracking-wider uppercase">
            HEAD-TO-HEAD BENCHMARK
          </span>
        </div>
        <h2 className="text-2xl sm:text-3xl lg:text-[38px] font-bold text-[#F0F6FC] tracking-tight">
          Why Choose Kvllay Over Redis?
        </h2>
        <p className="text-sm sm:text-base text-[#8B9BB4] max-w-[780px] leading-[1.6]">
          Redis is great, but often overkill. Kvllay delivers exact RESP2 compatibility with a fraction of the overhead, making it 87x smaller, 3.7x lighter, and faster than Redis across both single-client and multi-threaded workloads.
        </p>
      </Reveal>

      <div className="w-full grid grid-cols-1 md:grid-cols-3 gap-3.5 sm:gap-5 mb-10 sm:mb-14">
        {highlights.map((item, idx) => {
          const Icon = item.icon
          return (
            <Reveal key={item.title} direction="up" delay={idx * 80} className="h-full">
              <div className="h-full bg-[#0E131F] border border-[#1B2436] hover:border-[#1E3B5C] rounded-xl p-4 sm:p-6 flex flex-col justify-between gap-3.5 sm:gap-4 transition-colors shadow-sm">
                <div>
                  <div className="w-9 sm:w-10 h-9 sm:h-10 rounded-xl bg-[#00D2FF]/10 flex items-center justify-center mb-3 sm:mb-4">
                    <Icon className="size-4.5 sm:size-5 text-[#00D2FF]" />
                  </div>
                  <h3 className="text-base sm:text-lg font-bold text-[#F0F6FC] mb-1.5 sm:mb-2">{item.title}</h3>
                  <p className="text-[13px] sm:text-[13.5px] text-[#8B9BB4] leading-[1.55]">{item.desc}</p>
                </div>
                <div className="flex items-center gap-2 px-2.5 sm:px-3 py-1.5 rounded-md bg-[#0A0E17] border border-[#1B2436] text-xs font-mono">
                  <span className="text-[#00D2FF] font-bold">{item.badgeHighlight}</span>
                  <span className="text-[#546682]">{item.badgeText}</span>
                </div>
              </div>
            </Reveal>
          )
        })}
      </div>

      <Reveal direction="up" delay={50} className="w-full flex flex-col gap-4 sm:gap-6 mb-10 sm:mb-14 scroll-mt-24" as="div">
        <div id={paths.sections.benchmarks} className="w-full flex flex-col gap-4 sm:gap-6">
          <div className="flex flex-col sm:flex-row items-start sm:items-center justify-between gap-3 sm:gap-4 pb-1 sm:pb-2">
            <div>
              <h3 className="text-lg sm:text-2xl font-bold text-[#F0F6FC]">Visual Performance Graphs</h3>
              <p className="text-xs sm:text-[13px] text-[#8B9BB4] mt-0.5">
                Official benchmark results against Redis 8.x (redis-benchmark & stress tests)
              </p>
            </div>

            <div className="flex items-center gap-4 sm:gap-5 w-full sm:w-auto justify-between sm:justify-end">
              <div className="flex items-center gap-3 text-xs font-mono">
                <div className="flex items-center gap-1.5">
                  <span className="w-2.5 h-2.5 rounded-sm bg-[#00D2FF]" />
                  <span className="text-[#F0F6FC]">kvllay</span>
                </div>
                <div className="flex items-center gap-1.5">
                  <span className="w-2.5 h-2.5 rounded-sm bg-[#D53026]" />
                  <span className="text-[#8B9BB4]">Redis 8.x</span>
                </div>
              </div>

              <div className="flex items-center gap-2">
                <span className="text-xs font-mono text-[#546682] mr-1">
                  {String(activeSlide + 1).padStart(2, "0")} / {String(slides.length).padStart(2, "0")}
                </span>
                <button
                  type="button"
                  onClick={prevSlide}
                  aria-label="Previous Benchmark Chart"
                  className="p-1.5 sm:p-2 rounded-lg bg-[#0E131F] border border-[#1B2436] hover:border-[#00D2FF]/50 text-[#8B9BB4] hover:text-[#00D2FF] transition-colors focus:outline-none"
                >
                  <IconChevronLeft className="size-4" />
                </button>
                <button
                  type="button"
                  onClick={nextSlide}
                  aria-label="Next Benchmark Chart"
                  className="p-1.5 sm:p-2 rounded-lg bg-[#0E131F] border border-[#1B2436] hover:border-[#00D2FF]/50 text-[#8B9BB4] hover:text-[#00D2FF] transition-colors focus:outline-none"
                >
                  <IconChevronRight className="size-4" />
                </button>
              </div>
            </div>
          </div>

          <div className="flex items-center gap-2 overflow-x-auto pb-1 scrollbar-none">
            {slides.map((slide, idx) => (
              <button
                key={slide.id}
                type="button"
                onClick={() => setActiveSlide(idx)}
                className={`px-3 py-1.5 rounded-lg text-xs font-mono font-medium transition-all whitespace-nowrap ${
                  activeSlide === idx
                    ? "bg-[#0E131F] text-[#00D2FF] border border-[#00D2FF]/60 shadow-sm"
                    : "bg-[#0A0E17] text-[#8B9BB4] border border-[#1B2436] hover:text-[#F0F6FC]"
                }`}
              >
                {slide.tabName}
              </button>
            ))}
          </div>

          <div className="w-full flex flex-col gap-3 sm:gap-4">
            <div className="flex flex-col sm:flex-row sm:items-center justify-between gap-1.5 sm:gap-2">
              <div>
                <h4 className="text-sm sm:text-lg font-bold text-[#F0F6FC]">
                  {slides[activeSlide].title}
                </h4>
                <p className="text-[11.5px] sm:text-[13px] text-[#8B9BB4]">
                  {slides[activeSlide].subtitle}
                </p>
              </div>
              <div className="flex items-center gap-2">
                <span className="px-2 sm:px-2.5 py-0.5 sm:py-1 rounded-full bg-[#00D2FF]/10 border border-[#00D2FF]/30 text-[11px] sm:text-xs font-mono font-bold text-[#00D2FF]">
                  {slides[activeSlide].badge}
                </span>
              </div>
            </div>

            <div className="w-full flex items-center justify-center overflow-hidden py-1 sm:py-2">
              <img
                src={slides[activeSlide].image}
                alt={slides[activeSlide].title}
                className="max-h-[380px] sm:max-h-[480px] w-auto max-w-full object-contain transition-all duration-300"
              />
            </div>

            <div className="flex items-center justify-between gap-4 pt-1">
              <span className="text-[11px] sm:text-xs font-mono text-[#546682] truncate">
                {slides[activeSlide].stats}
              </span>

              <div className="flex items-center gap-1.5 shrink-0">
                {slides.map((_, idx) => (
                  <button
                    key={idx}
                    type="button"
                    onClick={() => setActiveSlide(idx)}
                    aria-label={`Jump to slide ${idx + 1}`}
                    className={`h-1.5 rounded-full transition-all focus:outline-none ${
                      activeSlide === idx ? "w-6 bg-[#00D2FF]" : "w-1.5 bg-[#1B2436] hover:bg-[#546682]"
                    }`}
                  />
                ))}
              </div>
            </div>
          </div>
        </div>
      </Reveal>

      <Reveal direction="up" delay={100} className="w-full bg-[#0E131F] border border-[#1B2436] rounded-xl overflow-hidden shadow-sm">
        <div className="overflow-x-auto">
          <table className="w-full text-left border-collapse min-w-[640px]">
            <thead>
              <tr className="bg-[#0A0E17] border-b border-[#1B2436] text-[11px] sm:text-[12px] font-mono">
                <th className="py-3 px-4 sm:py-3.5 sm:px-6 font-bold text-[#546682]">WORKLOAD / METRIC</th>
                <th className="py-3 px-4 sm:py-3.5 sm:px-6 font-bold text-[#00D2FF]">KVLLAY V1.0.0</th>
                <th className="py-3 px-4 sm:py-3.5 sm:px-6 font-bold text-[#8B9BB4]">REDIS 8.x</th>
                <th className="py-3 px-4 sm:py-3.5 sm:px-6 font-bold text-[#546682]">COMPARISON RESULT</th>
              </tr>
            </thead>
            <tbody className="divide-y divide-[#1B2436] text-xs sm:text-[13px] font-mono">
              {tableData.map((row, idx) => (
                <tr key={idx} className="hover:bg-[#121826] transition-colors">
                  <td className="py-3 px-4 sm:py-3.5 sm:px-6 font-sans font-medium text-[#F0F6FC]">{row.metric}</td>
                  <td className="py-3 px-4 sm:py-3.5 sm:px-6 font-bold text-[#00D2FF]">{row.kvllay}</td>
                  <td className="py-3 px-4 sm:py-3.5 sm:px-6 text-[#8B9BB4]">{row.redis}</td>
                  <td className="py-3 px-4 sm:py-3.5 sm:px-6 text-[#10B981] font-semibold">{row.result}</td>
                </tr>
              ))}
            </tbody>
          </table>
        </div>
      </Reveal>
    </section>
  )
}
