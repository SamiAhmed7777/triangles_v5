class Triangles < Formula
  desc "Cryptographic Triangles (TRI) cryptocurrency wallet and daemon"
  homepage "https://cryptographic-triangles.org"
  license "MIT"
  version "5.3.6"

  on_macos do
    url "https://github.com/SamiAhmed7777/triangles_v5/releases/download/v5.3.6/Cryptographic-Triangles-v5.3.6-macos-arm64.dmg"
    sha256 "3a58e795d898656b455fd639c0ea826a4457d390a64d00ace9a1257598d053be"
  end

  on_linux do
    url "https://github.com/SamiAhmed7777/triangles_v5/releases/download/v5.3.6/Cryptographic-Triangles-v5.3.6-linux-x64-daemon"
    sha256 "4d2ab25d61127d6aff3e6f3069556d04f4b823f8849e97629c12871ad4779517"
  end

  depends_on "openssl@3"

  def install
    if OS.mac?
      prefix.install "Triangles-Qt.app"
      bin.write_exec_script prefix/"Triangles-Qt.app/Contents/MacOS/Triangles-Qt"
    else
      bin.install "Cryptographic-Triangles-v5.3.6-linux-x64-daemon" => "trianglesd"
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
