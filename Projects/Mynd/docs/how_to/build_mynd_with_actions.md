# Building the MYND Firmware with GitHub Actions

#### Step 1: Fork and Access Actions

- Fork the `mynd-firmware` repository

#### Step 2: Make a Change and Push to `main`

- Edit code directly in the GitHub web IDE by replacing `.com` with `.dev` in the URL:  
    `https://github.com/<your_gh_username>/mynd-firmware` → `https://github.dev/<your_gh_username>/mynd-firmware`
  - Or, clone the repo to develop locally with a preferred IDE and push to `main` using Git (`git push origin main`)
- The workflow is configured to run automatically on pushes and pull requests to the `main` branch and will build the MYND firmware binaries from the `main` branch
    - **Tip:** If you wish to run this pipeline on branches other than `main`, edit the workflow by adding it to the comma-separated list `branches: ["main"]` in [build-firmware.yml](../../../../.github/workflows/build-firmware.yml)
  - You can monitor the pipeline's progress by clicking on the **Actions** tab and selecting the **Build MYND Firmware** workflow from the left pane

#### Step 3: Download Artifacts

- Navigate to the **Actions** tab at the top of your `mynd-firmware` GitHub webpage
- After the workflow completes, click on the latest pipeline
- Wait a few minutes for build artifacts to appear under the **Artifacts** section at the bottom of the page and select the download option

#### Step 4: Perform Drag & Drop MCU Update
- Follow the [How To guide](./perform_drag&drop_update.md) for performing the drag & drop update
