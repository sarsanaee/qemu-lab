import os
import re
import requests
import subprocess
from bs4 import BeautifulSoup
from urllib.parse import urljoin

def fetch_patch_links_lore(cover_letter_url):
    """Fetch all patch links matching '[PATCH vX M/N]' from the lore.kernel.org cover letter."""
    try:
        response = requests.get(cover_letter_url)
        response.raise_for_status()

        # Parse the HTML content
        soup = BeautifulSoup(response.text, 'html.parser')

        # Regex pattern for '[PATCH vX M/N]' where M > 0
        patch_pattern = re.compile(r"\[PATCH.* [1-9]\d*/\d+\]")

        # Find links matching the pattern
        patch_links = []
        for link in soup.find_all('a', href=True):
            if patch_pattern.search(link.text):  # Match the pattern in the anchor text
                full_url = urljoin(cover_letter_url, link['href'])  # Ensure absolute URL
                patch_links.append(full_url)

        # Remove duplicates and sort
        patch_links = sorted(set(patch_links))
        print(f"Found patch links: {patch_links}")
        return patch_links
    except requests.exceptions.RequestException as e:
        print(f"Error fetching cover letter: {e}")
        return []

def download_patch(patch_url, output_dir):
    """Download an individual patch in raw format."""
    try:
        # Ensure the patch URL points to the raw content
        #  raw_patch_url = f"{patch_url}/raw"
        raw_patch_url = f"{patch_url.rstrip('/')}/raw"  # Safely append /raw
        response = requests.get(raw_patch_url)
        response.raise_for_status()
        
        # Save the patch file
        patch_name = os.path.basename(patch_url) + ".patch"  # Append .patch for clarity
        patch_path = os.path.join(output_dir, patch_name)
        with open(patch_path, 'wb') as f:
            f.write(response.content)
        print(f"Downloaded patch: {patch_path}")
        return patch_path
    except requests.exceptions.RequestException as e:
        print(f"Error downloading patch: {e}")
        return None

def apply_patch(patch_path, repo_dir):
    """Apply a single patch to the repository."""
    try:
        subprocess.run(
            ["git", "am", patch_path],
            cwd=repo_dir,
            check=True
        )
        print(f"Patch applied successfully: {patch_path}")
    except subprocess.CalledProcessError as e:
        print(f"Error applying patch {patch_path}: {e}")

def main():
    cover_letter_url = input("Enter the URL of the patchset cover letter (0/n): ")
    repo_dir = input("Enter the path to the repository: ")

    # Ensure the repository exists
    if not os.path.isdir(repo_dir):
        print(f"Repository directory {repo_dir} does not exist.")
        return

    # Ensure output directory for patches
    output_dir = os.path.join(repo_dir, "patches")
    os.makedirs(output_dir, exist_ok=True)

    # Fetch all patch links from the cover letter
    patch_links = fetch_patch_links_lore(cover_letter_url)
    if not patch_links:
        print("No patches found in the cover letter.")
        return

    # Download and apply patches sequentially
    for patch_url in patch_links:
        patch_path = download_patch(patch_url, output_dir)
        if patch_path:
            apply_patch(patch_path, repo_dir)

if __name__ == "__main__":
    main()
