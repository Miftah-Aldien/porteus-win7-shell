v5.2 - PERBAIKAN BUILD GAGAL v5.1

Penyebab gagal v5.1: zip-nya bersarang porteus-win7-shell-v51/src/main.cpp, sedangkan workflow GitHub nyari src/main.cpp di root repo. Jadi g++ bilang file tidak ada.
Di v5.2 ini struktur sudah RATA: extract zip -> langsung ada src/main.cpp, .github/workflows/build-i686.yml, dll di root.

Cara pakai yang benar:
1. Rename .ttf -> .zip, extract
2. Isi hasil extract (src/, .github/, dll) COPY ke root repo GitHub-mu, bukan copy foldernya.
3. Push, tunggu build. Cek step 'Build i686' - kalau hijau, ambil artifact win7shell-linux-i686.
4. Peringatan 'Node 20 is deprecated' di log itu BUKAN error, abaikan saja.

Isi sama dengan v5.1: Foto hanya referensi tampilan, isi Start Menu dari Wine, fallback netral, picom opsional (Openbox + Picom cocok, tapi tidak wajib).
