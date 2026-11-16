class BraveHistoryMcp < Formula
  desc "Search Brave browser history (titles, URLs, visits) over MCP"
  homepage "https://github.com/bdombro/brave-history-mcp"
  version "2.1.0"

  on_macos do
    url "https://github.com/bdombro/brave-history-mcp/releases/download/v2.1.0/brave-history-mcp-arm64-macos.tar.gz"
    sha256 "197f4543cd07414edc295aefa1163934c1de5a02f16463f19181ff41339b5988"
  end

  def install
    bin.install "brave-history-mcp"
    chmod 0755, bin/"brave-history-mcp"
    pkgshare.install "docs" if File.directory?("docs")
  end

  def post_install
    (var/"brave-history-mcp").mkpath
    chmod 0700, var/"brave-history-mcp"
    (var/"log").mkpath
  end

  def caveats
    <<~EOS
      Reading Brave Browser history on macOS may require Full Disk Access or
      App Data permission for #{opt_bin}/brave-history-mcp.
    EOS
  end

  service do
    run ["/bin/sh", "-c", "[ -f #{etc}/brave-history-mcp/env ] && set -a && . #{etc}/brave-history-mcp/env; exec #{opt_bin}/brave-history-mcp http --log-file #{var}/log/brave-history-mcp.log"]
    keep_alive true
    working_dir var/"brave-history-mcp"
    log_path var/"log/brave-history-mcp.log"
    error_log_path var/"log/brave-history-mcp.log"
    environment_variables PATH: "#{HOMEBREW_PREFIX}/bin:/usr/bin:/bin"
  end

  test do
    assert_match version.to_s, shell_output("#{bin}/brave-history-mcp --version")
    discover = '{"jsonrpc":"2.0","id":1,"method":"server/discover","params":{"_meta":{"io.modelcontextprotocol/protocolVersion":"2026-07-28","io.modelcontextprotocol/clientCapabilities":{}}}}'
    out = pipe_output("#{bin}/brave-history-mcp mcp --skip-checks", "#{discover}\n")
    assert_match "brave-history-mcp", out
  end
end
