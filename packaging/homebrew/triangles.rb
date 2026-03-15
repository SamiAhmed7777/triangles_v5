class Triangles < Formula
  desc "Cryptographic Triangles (TRI) cryptocurrency wallet and daemon"
  homepage "https://cryptographic-triangles.org"
  license "MIT"
  version "5.1.4"

  on_macos do
    if Hardware::CPU.intel?
      url "https://github.com/SamiAhmed7777/triangles_v5/releases/download/v5.1.4/Cryptographic-Triangles-v5.1.4-macos-x64.dmg"
      sha256 "PLACEHOLDER_X64_HASH"
    else
      url "https://github.com/SamiAhmed7777/triangles_v5/releases/download/v5.1.4/Cryptographic-Triangles-v5.1.4-macos-arm64.dmg"
      sha256 "PLACEHOLDER_ARM64_HASH"
    end
  end

  on_linux do
    url "https://github.com/SamiAhmed7777/triangles_v5/releases/download/v5.1.4/trianglesd-linux"
    sha256 "6f5c19d34a2e1f6cdadee095d9e11b25d18b41a0d1602a163ffca7ec80b3da37"
  end

  depends_on "openssl@3"

  def install
    if OS.mac?
      prefix.install "Triangles-Qt.app"
      bin.write_exec_script prefix/"Triangles-Qt.app/Contents/MacOS/Triangles-Qt"
    else
      bin.install "trianglesd-linux" => "trianglesd"
    end
  end

  service do
    run [opt_bin/"trianglesd", "-daemon=0"]
    keep_alive true
    working_dir var/"triangles"
  end

  test do
    if OS.mac?
      assert_predicate prefix/"Triangles-Qt.app", :exist?
    else
      system "#{bin}/trianglesd", "-version"
    end
  end
end
