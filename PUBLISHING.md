# Publishing from the GitHub website

1. Create a new empty repository named `Progress-Keeper`.
2. Choose **Add file**, then **Upload files**.
3. Upload the contents of this project so `mod.json` and `CMakeLists.txt` are at the repository root.
4. Create `.github/workflows/build.yml` through **Add file**, then **Create new file**, if the website did not preserve the hidden folder during upload.
5. Open the **Actions** tab and wait for Windows, macOS, iOS, Android32, Android64, and the package job to become green.
6. Open the successful run and download the `Progress-Keeper-All-Platforms` artifact.
7. Test the combined `.geode` package on the platforms you can access before making a release.
8. Open **Releases**, choose **Draft a new release**, create tag `v1.0.0`, and upload the tested `.geode` file.

Read the current Geode Mod Guidelines before requesting index publication. A successful build does not guarantee index approval.
