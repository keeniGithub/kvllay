import { IconTerminal2, IconShield, IconClock, IconLock, IconBox, IconDeviceDesktop } from "@tabler/icons-react"
import { paths } from "@/config/paths"

export function FeaturesBento() {
  const features = [
    {
      icon: IconTerminal2,
      title: "100% RESP2 Compatible",
      desc: "Implements Redis RESP2 serialization and inline commands. Seamlessly handles arrays, bulk strings, integers, and errors so your existing code just works.",
    },
    {
      icon: IconShield,
      title: "Thread-Safe Concurrency",
      desc: "Fine-grained std::shared_mutex allows unlimited parallel concurrent readers on GET/EXISTS, while safely serializing writes on SET and DEL without corruption.",
    },
    {
      icon: IconClock,
      title: "TTL & Hybrid Eviction",
      desc: "Full EXPIRE, PEXPIRE, TTL, PTTL, and PERSIST support. Combines instant lazy cleanup on read requests with a passive background garbage collector thread.",
    },
    {
      icon: IconLock,
      title: "Security & Authentication",
      desc: "Configure password protection via requirepass / -a. Enforces exact authorization with standard NOAUTH and WRONGPASS errors strictly before command execution.",
    },
    {
      icon: IconBox,
      title: "1.6 MB Scratch Container",
      desc: "Multi-stage Docker scratch build generates an ultra-lean binary with zero glibc or shared library dependencies. Pull in under 1 second anywhere.",
    },
    {
      icon: IconDeviceDesktop,
      title: "Cross-Platform Native",
      desc: "Unified modern socket engine: POSIX sockets on Linux, Winsock2 on Windows. Standalone pre-compiled executables available for both with zero setup.",
    },
  ]

  return (
    <section id={paths.sections.features} className="w-full flex flex-col items-center py-10 sm:py-16 px-4 sm:px-6 lg:px-10 max-w-[1240px] mx-auto scroll-mt-20">
      <div className="flex flex-col items-center text-center gap-2.5 sm:gap-3 mb-8 sm:mb-12">
        <div className="inline-flex items-center gap-2 px-3 py-1 rounded-full bg-[#00D2FF]/10 border border-[#00D2FF]/30">
          <span className="text-[11px] font-mono font-bold text-[#00D2FF] tracking-wider uppercase">
            ENGINEERING ARCHITECTURE
          </span>
        </div>
        <h2 className="text-2xl sm:text-3xl lg:text-[38px] font-bold text-[#F0F6FC] tracking-tight">
          Built for Speed, Simplicity &amp; Reliability
        </h2>
        <p className="text-sm sm:text-base text-[#8B9BB4] max-w-[780px] leading-[1.6]">
          Every subsystem is hand-crafted in modern C++17 to eliminate bloat, guarantee safety, and deliver predictable ultra-low latency.
        </p>
      </div>

      <div className="w-full grid grid-cols-1 md:grid-cols-2 lg:grid-cols-3 gap-3.5 sm:gap-5">
        {features.map((item) => {
          const Icon = item.icon
          return (
            <div
              key={item.title}
              className="bg-[#0E131F] border border-[#1B2436] hover:border-[#1E3B5C] rounded-xl p-4 sm:p-6 flex flex-col justify-start gap-2.5 sm:gap-3.5 transition-colors shadow-sm group"
            >
              <div className="w-9 h-9 rounded-lg bg-[#0A0E17] border border-[#1B2436] group-hover:border-[#00D2FF]/40 flex items-center justify-center transition-colors">
                <Icon className="size-4.5 text-[#00D2FF]" />
              </div>
              <h3 className="text-[16px] sm:text-[17px] font-bold text-[#F0F6FC]">{item.title}</h3>
              <p className="text-[13px] sm:text-[13.5px] text-[#8B9BB4] leading-[1.55]">{item.desc}</p>
            </div>
          )
        })}
      </div>
    </section>
  )
}
