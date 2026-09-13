import logoImg from "@root/logo.png"
import { links } from "@/config/links"

export function Footer() {
  return (
    <footer className="w-full border-t border-[#1B2436] bg-[#070A10] mt-auto">
      <div className="max-w-[1240px] mx-auto px-4 sm:px-6 lg:px-10 py-8 sm:py-12 flex flex-col md:flex-row justify-between items-start gap-8 md:gap-10">
        <div className="flex flex-col gap-2.5 max-w-[320px]">
          <div className="flex items-center gap-2.5">
            <div className="w-7.5 sm:w-8 h-7.5 sm:h-8 rounded-lg overflow-hidden flex items-center justify-center">
              <img src={logoImg} alt="kvllay logo" className="w-full h-full object-contain" />
            </div>
            <span className="text-base sm:text-lg font-bold text-[#F0F6FC] tracking-tight">kvllay</span>
          </div>
          <p className="text-[12.5px] sm:text-[13px] text-[#8B9BB4] leading-relaxed">
            Lightweight in-memory key-value database in modern C++17.
          </p>
          <p className="text-xs text-[#546682] pt-0.5 sm:pt-1">
            © 2026 Kenyka • MIT Licensed
          </p>
        </div>

        <div className="grid grid-cols-2 sm:grid-cols-3 gap-6 sm:gap-12 w-full md:w-auto">
          <div className="flex flex-col gap-2 sm:gap-2.5">
            <span className="text-[10.5px] sm:text-[11px] font-mono font-bold text-[#546682] tracking-wider uppercase">
              PROJECT
            </span>
            <a
              href={links.github}
              target="_blank"
              rel="noreferrer"
              className="text-[12.5px] sm:text-[13px] text-[#8B9BB4] hover:text-[#00D2FF] transition-colors"
            >
              GitHub Repository
            </a>
            <a
              href={links.releases}
              target="_blank"
              rel="noreferrer"
              className="text-[12.5px] sm:text-[13px] text-[#8B9BB4] hover:text-[#00D2FF] transition-colors"
            >
              Releases
            </a>
            <a
              href={links.license}
              target="_blank"
              rel="noreferrer"
              className="text-[12.5px] sm:text-[13px] text-[#8B9BB4] hover:text-[#00D2FF] transition-colors"
            >
              MIT License
            </a>
          </div>

          <div className="flex flex-col gap-2 sm:gap-2.5">
            <span className="text-[10.5px] sm:text-[11px] font-mono font-bold text-[#546682] tracking-wider uppercase">
              DOCUMENTATION
            </span>
            <a
              href={links.docsRu}
              target="_blank"
              rel="noreferrer"
              className="text-[12.5px] sm:text-[13px] text-[#8B9BB4] hover:text-[#00D2FF] transition-colors"
            >
              Russian Guide (ru.md)
            </a>
            <a
              href={links.docsEn}
              target="_blank"
              rel="noreferrer"
              className="text-[12.5px] sm:text-[13px] text-[#8B9BB4] hover:text-[#00D2FF] transition-colors"
            >
              English Guide (en.md)
            </a>
            <a
              href={links.redisProtocolSpec}
              target="_blank"
              rel="noreferrer"
              className="text-[12.5px] sm:text-[13px] text-[#8B9BB4] hover:text-[#00D2FF] transition-colors"
            >
              RESP2 Specification
            </a>
          </div>

          <div className="flex flex-col gap-2 sm:gap-2.5 col-span-2 sm:col-span-1">
            <span className="text-[10.5px] sm:text-[11px] font-mono font-bold text-[#546682] tracking-wider uppercase">
              COMMUNITY
            </span>
            <a
              href={links.issues}
              target="_blank"
              rel="noreferrer"
              className="text-[12.5px] sm:text-[13px] text-[#8B9BB4] hover:text-[#00D2FF] transition-colors"
            >
              Report an Issue
            </a>
            <a
              href={links.pulls}
              target="_blank"
              rel="noreferrer"
              className="text-[12.5px] sm:text-[13px] text-[#8B9BB4] hover:text-[#00D2FF] transition-colors"
            >
              Contribute Code
            </a>
            <a
              href={links.dockerHub}
              target="_blank"
              rel="noreferrer"
              className="text-[12.5px] sm:text-[13px] text-[#8B9BB4] hover:text-[#00D2FF] transition-colors"
            >
              Docker Hub Image
            </a>
          </div>
        </div>
      </div>
    </footer>
  )
}
