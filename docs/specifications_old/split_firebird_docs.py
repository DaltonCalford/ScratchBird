import re
import os
from pypdf import PdfReader

# Configuration
INPUT_FILE = "firebird-50-language-reference.pdf"
OUTPUT_DIR = "firebird_docs_split"

# Chapter map based on the Table of Contents
# Format: ("Chapter Name", Start_Page)
# Note: Page numbers are approximate based on the ToC text.
# The script will read from Start_Page up to the next Chapter's Start_Page.
CHAPTERS = [
    ("00_Preface_and_ToC", 1),
    ("01_About_Firebird_5.0", 18),
    ("02_SQL_Language_Structure", 20),
    ("03_Data_Types_and_Subtypes", 27),
    ("04_Common_Language_Elements", 69),
    ("05_DDL_Statements", 104),
    ("06_DML_Statements", 252),
    ("07_PSQL_Statements", 349),
    ("08_Built_in_Scalar_Functions", 411),
    ("09_Aggregate_Functions", 492),
    ("10_Window_Functions", 513),
    ("11_System_Packages", 531),
    ("12_Context_Variables", 542),
    ("13_Transaction_Control", 559),
    ("14_Security", 575),
    ("15_Management_Statements", 623),
    ("App_A_Supplementary_Info", 641),
    ("App_B_Exception_Codes", 644),
    ("App_C_Reserved_Words", 708),
    ("App_D_System_Tables", 715),
    ("App_E_Monitoring_Tables", 757),
    ("App_F_Security_Tables", 772),
    ("App_G_Plugin_Tables", 775),
    ("App_H_Charsets_and_Collations", 783),
    ("App_I_License", 789),
    ("App_J_Document_History", 790)
]

def clean_text(text):
    """
    Basic cleanup to remove headers/footers or excessive whitespace
    often found in PDF extractions.
    """
    # Remove repetitive page headers/footers if identifiable patterns exist
    # For now, we'll just trim and normalize newlines slightly
    text = re.sub(r'\n{3,}', '\n\n', text)
    return text.strip()

def split_pdf_to_markdown():
    if not os.path.exists(INPUT_FILE):
        print(f"Error: File {INPUT_FILE} not found.")
        return

    if not os.path.exists(OUTPUT_DIR):
        os.makedirs(OUTPUT_DIR)

    try:
        reader = PdfReader(INPUT_FILE)
        total_pages = len(reader.pages)
        print(f"Loaded PDF with {total_pages} pages.")

        for i, (chapter_name, start_page) in enumerate(CHAPTERS):
            # PDF pages are 0-indexed, ToC is 1-indexed
            start_idx = start_page - 1

            # Determine end page
            if i + 1 < len(CHAPTERS):
                end_idx = CHAPTERS[i+1][1] - 1
            else:
                end_idx = total_pages

            print(f"Processing {chapter_name} (Pages {start_page}-{end_idx})...")

            chapter_text = []

            # Extract text from page range
            for p in range(start_idx, end_idx):
                if p >= total_pages:
                    break
                page = reader.pages[p]
                text = page.extract_text()
                if text:
                    # Add a marker for original page number
                    chapter_text.append(f"\n\n<!-- Original PDF Page: {p+1} -->\n\n")
                    chapter_text.append(clean_text(text))

            # Save to file
            output_filename = os.path.join(OUTPUT_DIR, f"{chapter_name}.md")
            with open(output_filename, "w", encoding="utf-8") as f:
                f.write(f"# {chapter_name.replace('_', ' ')}\n\n")
                f.write("".join(chapter_text))

        print(f"\nSuccess! Documents created in folder: {OUTPUT_DIR}")

    except Exception as e:
        print(f"An error occurred: {e}")

if __name__ == "__main__":
    # Ensure pypdf is installed: pip install pypdf
    split_pdf_to_markdown()
