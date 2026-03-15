class Triangles < Formula
  desc "Cryptographic Triangles (TRI) cryptocurrency wallet and daemon"
  homepage "https://cryptographic-triangles.org"
  license "MIT"
  version "5.1.4"

  on_linux do
    url "https://github.com/SamiAhmed7777/triangles_v5/releases/download/v5.1.4/trianglesd-linux"
    sha256 "6f5c19d34a2e1f6cdadee095d9e11b25d18b41a0d1602a163ffca7ec80b3da37"
  end

  depends_on "openssl@3"
  depends_on :linux

  def install
    bin.install "trianglesd-linux" => "trianglesd"
  end

  service do
    run [opt_bin/"trianglesd", "-daemon=0"]
    keep_alive true
    working_dir var/"triangles"
  end

  test do
    system "#{bin}/trianglesd", "-version"
  end
end
