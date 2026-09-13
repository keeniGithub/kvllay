import { useState, useEffect } from "react"

const CACHE_KEY = "kvllay_github_stars"
const CACHE_EXPIRY = 1000 * 60 * 30

function getCachedStars(): number | null {
  try {
    const cached = sessionStorage.getItem(CACHE_KEY)
    if (cached) {
      const { value, timestamp } = JSON.parse(cached)
      if (Date.now() - timestamp < CACHE_EXPIRY && typeof value === "number") {
        return value
      }
    }
  } catch {
    return null
  }
  return null
}

export function useGithubStars(owner = "keeniGithub", repo = "kvllay") {
  const [stars, setStars] = useState<number | null>(getCachedStars)
  const [isLoading, setIsLoading] = useState(() => getCachedStars() === null)

  useEffect(() => {
    if (getCachedStars() !== null) {
      return
    }

    let isMounted = true

    fetch(`https://api.github.com/repos/${owner}/${repo}`)
      .then((res) => {
        if (!res.ok) throw new Error("Failed to fetch GitHub repo data")
        return res.json()
      })
      .then((data) => {
        if (!isMounted) return
        if (data && typeof data.stargazers_count === "number") {
          setStars(data.stargazers_count)
          try {
            sessionStorage.setItem(
              CACHE_KEY,
              JSON.stringify({ value: data.stargazers_count, timestamp: Date.now() })
            )
          } catch (e) {
            void e
          }
        }
      })
      .catch((err) => {
        console.warn("Could not fetch stars:", err)
      })
      .finally(() => {
        if (isMounted) setIsLoading(false)
      })

    return () => {
      isMounted = false
    }
  }, [owner, repo])

  return { stars, isLoading }
}

export function formatStars(count: number | null): string {
  if (count === null) return "0"
  if (count >= 1000) {
    return `${(count / 1000).toFixed(1)}k`
  }
  return String(count)
}
