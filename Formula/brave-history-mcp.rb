class BraveHistoryMcp < Formula
  desc "Search Brave browser history (titles, URLs, visits) over MCP"
  homepage "https://github.com/bdombro/brave-history-mcp"
  version "2.2.2"

  on_macos do
    url "https://github.com/bdombro/brave-history-mcp/releases/download/v2.2.2/brave-history-mcp-arm64-macos.tar.gz"
    sha256 "9021e93403792000c8850ab96079305e3925ed0211e8c3b157eb507758115062"
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
    require "net/http"
    assert_match version.to_s, shell_output("#{bin}/brave-history-mcp --version")
    r, w = IO.pipe
    pid = spawn("#{bin}/brave-history-mcp", "--skip-checks", "http", "--host", "127.0.0.1", "--port", "0", err: w)
    w.close
    begin
      port = nil
      r.each_line { |line| break if (port = line[%r{listening on http://[\d.]+:(\d+)/mcp}, 1]) }
      assert port, "server did not report a listening port"
      discover = '{"jsonrpc":"2.0","id":1,"method":"server/discover","params":{"_meta":{"io.modelcontextprotocol/protocolVersion":"2026-07-28","io.modelcontextprotocol/clientCapabilities":{}}}}'
      res = Net::HTTP.post(URI("http://127.0.0.1:#{port}/mcp"), discover,
                           "Content-Type"         => "application/json",
                           "Accept"               => "application/json, text/event-stream",
                           "MCP-Protocol-Version" => "2026-07-28",
                           "Mcp-Method"           => "server/discover")
      assert_match "brave-history-mcp", res.body
    ensure
      Process.kill("TERM", pid)
      Process.wait(pid)
    end
  end
end
