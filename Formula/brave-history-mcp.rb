class BraveHistoryMcp < Formula
  desc "Search Brave browser history (titles, URLs, visits) over MCP"
  homepage "https://github.com/bdombro/brave-history-mcp"
  url "https://github.com/bdombro/brave-history-mcp/releases/download/v2.0.0/brave-history-mcp.zip"
  version "2.0.0"
  sha256 "0000000000000000000000000000000000000000000000000000000000000000"
  license "ISC"

  def install
    bin.install "brave-history-mcp"
    chmod 0755, bin/"brave-history-mcp"
  end

  service do
    run [opt_bin/"brave-history-mcp", "http", "--host", "127.0.0.1", "--port", "8083"]
    keep_alive true
    log_path var/"log/brave-history-mcp.log"
    error_log_path var/"log/brave-history-mcp.log"
  end

  test do
    output = shell_output("#{bin}/brave-history-mcp status")
    assert_match version.to_s, output
  end
end
