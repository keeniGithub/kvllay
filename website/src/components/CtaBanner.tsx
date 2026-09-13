import { Star, Download, BookOpen } from "lucide-react"
import { useGithubStars, formatStars } from "@/lib/useGithubStars"

export function CtaBanner() {
  const { stars, isLoading } = useGithubStars()

  return (
    <section className="w-full py-12 px-6 lg:px-10 max-w-[1240px] mx-auto">
      <div className="w-full bg-[#0E131F] border border-[#1E3B5C] rounded-2xl p-8 sm:p-12 lg:p-14 flex flex-col items-center text-center gap-6 shadow-[0_25px_60px_rgba(0,0,0,0.6)] relative overflow-hidden">
        <div className="absolute -top-24 left-1/2 -translate-x-1/2 w-96 h-48 bg-[#00D2FF]/10 rounded-full blur-3xl pointer-events-none" />

        <div className="relative w-24 sm:w-28 h-auto object-contain drop-shadow-[0_8px_24px_rgba(0,210,255,0.35)]">
          <img
            src="/allay_fly.webp"
            alt="Kvllay Flying Allay Animated Mascot"
            className="w-full h-auto object-contain"
          />
        </div>

        <div className="inline-flex items-center gap-2 px-3 py-1 rounded-full bg-[#00D2FF]/10 border border-[#00D2FF]/30">
          <span className="text-[11px] font-mono font-bold text-[#00D2FF] tracking-wider uppercase">
            OPEN SOURCE • MIT LICENSE
          </span>
        </div>

        <div className="flex flex-col items-center gap-3 max-w-[760px]">
          <h2 className="text-2xl sm:text-3xl lg:text-4xl font-bold text-[#F0F6FC] tracking-tight leading-tight">
            Ready for a 90x Lighter, Blazing-Fast Cache?
          </h2>
          <p className="text-sm sm:text-base text-[#8B9BB4] leading-[1.6]">
            Join developers swapping out heavy Redis containers for Kvllay in local dev environments, microservices, and edge computing. Zero migration code required.
          </p>
        </div>

        <div className="flex flex-wrap items-center justify-center gap-3.5 pt-2 w-full">
          <a
            href="https://github.com/keeniGithub/kvllay"
            target="_blank"
            rel="noreferrer"
            className="flex items-center justify-center gap-2.5 px-6 py-3.5 bg-[#00D2FF] hover:bg-[#00b8e6] text-[#08090E] rounded-xl text-[14px] sm:text-[15px] font-bold transition-all shadow-[0_0_25px_rgba(0,210,255,0.25)] hover:shadow-[0_0_30px_rgba(0,210,255,0.4)] w-full sm:w-auto"
          >
            <Star className="size-4.5 fill-[#08090E]" />
            <span>Star on GitHub</span>
            {isLoading ? (
              <span className="ml-1 w-10 h-5 rounded-full bg-[#08090E]/20 animate-pulse inline-block" />
            ) : (
              <span className="ml-1 px-2 py-0.5 rounded-full bg-[#08090E]/20 text-xs font-mono text-[#08090E] font-bold">
                {formatStars(stars)}
              </span>
            )}
          </a>

          <a
            href="https://github.com/keeniGithub/kvllay/releases"
            target="_blank"
            rel="noreferrer"
            className="flex items-center justify-center gap-2 px-5 py-3.5 bg-[#0A0E17] hover:bg-[#161D2E] border border-[#1E3B5C] rounded-xl text-[14px] font-medium text-[#F0F6FC] transition-colors w-full sm:w-auto"
          >
            <Download className="size-4 text-[#F0F6FC]" />
            <span>GitHub Releases</span>
          </a>

          <a
            href="https://github.com/keeniGithub/kvllay/blob/master/README.md"
            target="_blank"
            rel="noreferrer"
            className="flex items-center justify-center gap-2 px-5 py-3.5 bg-[#0A0E17] hover:bg-[#161D2E] border border-[#1E3B5C] rounded-xl text-[14px] font-medium text-[#8B9BB4] hover:text-[#F0F6FC] transition-colors w-full sm:w-auto"
          >
            <BookOpen className="size-4 text-[#8B9BB4]" />
            <span>Documentation (RU / EN)</span>
          </a>
        </div>
      </div>
    </section>
  )
}
