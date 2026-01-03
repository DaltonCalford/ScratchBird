# ScratchBird Logo Usage Guide

**Quick Reference for Documentation Authors**
**Last Updated:** 2026-01-03

---

## Logo Files Available

All logos are in `wiki/images/logos/`:

1. **TransparentScratchBirdLogoHeader.png** - ⭐ Primary header logo (transparent)
2. **ScratchBirdLogo.png** - Icon/avatar logo
3. **ScratchBirdLogoHeader.png** - Header with background
4. **LogoWork.svg** - Vector source file

---

## Common Uses

### Wiki Pages (Most Common)

Add header logo at the top of important wiki pages:

```markdown
![ScratchBird Logo](images/logos/TransparentScratchBirdLogoHeader.png)

# Page Title

Content goes here...
```

**Used on:**
- ✅ Home.md (wiki landing page)
- Getting-Started.md (recommended)
- installation/Docker.md (recommended)
- Major tutorial pages (recommended)

### Repository README Files

For README.md files in the main repository:

```markdown
![ScratchBird](docs/Artwork/TransparentScratchBirdLogoHeader.png)

# ScratchBird

High-performance database with Firebird MGA transaction model...
```

### Driver Documentation Headers

For driver-specific documentation:

```markdown
![ScratchBird](../../images/logos/TransparentScratchBirdLogoHeader.png)

# Python Driver for ScratchBird

Official Python driver implementing PEP 249 DB-API...
```

### Inline Icon References

Small logo for inline use:

```markdown
## ScratchBird Features

ScratchBird ![icon](images/logos/ScratchBirdLogo.png){ width=24px }
implements the Firebird MGA transaction model...
```

---

## Logo Sizing Guidelines

### Wiki Headers

**Recommended markdown:**
```markdown
![ScratchBird Logo](images/logos/TransparentScratchBirdLogoHeader.png)
```

**Renders at:**
- Full width on mobile
- Max ~800px on desktop
- Auto-scales based on viewport

### Resized Headers

If you need a smaller header:

```markdown
<img src="images/logos/TransparentScratchBirdLogoHeader.png" alt="ScratchBird" width="600">
```

Or create a custom size:

```markdown
<img src="images/logos/TransparentScratchBirdLogoHeader.png" alt="ScratchBird" style="max-width: 500px;">
```

### Small Icons

For inline icons (24x24 to 64x64):

```markdown
![ScratchBird](images/logos/ScratchBirdLogo.png){ width=32px }
```

Or HTML:

```html
<img src="images/logos/ScratchBirdLogo.png" alt="ScratchBird" width="32" height="32">
```

---

## Best Practices

### ✅ Do Use Logos On:

- **Wiki Home page** - Creates professional landing page
- **Getting Started guide** - Reinforces branding
- **Installation guides** - Visual appeal for first-time users
- **Tutorial headers** - Makes content more engaging
- **Major feature documentation** - Important sections
- **README files** - Project repositories
- **Presentations** - Slides and talks

### ❌ Don't Overuse:

- **Every single page** - Can become repetitive
- **Multiple times per page** - One header is enough
- **Inline with every mention** - Use sparingly
- **In code blocks** - Keep code clean
- **In tables** - Usually unnecessary

### 🎨 Design Tips:

1. **Use transparent version** for wiki pages (TransparentScratchBirdLogoHeader.png)
2. **Use solid background version** for emails or printed docs (ScratchBirdLogoHeader.png)
3. **Use icon version** for small spaces (ScratchBirdLogo.png)
4. **Consistent placement** - Top of page, before H1 title
5. **Add alt text** - Always include `alt="ScratchBird Logo"`

---

## Examples by Document Type

### Example: Wiki Home Page

```markdown
![ScratchBird Logo](images/logos/TransparentScratchBirdLogoHeader.png)

# Welcome to ScratchBird Wiki

**Version:** Beta 0.9.x
...
```

✅ **Result:** Professional, branded landing page

---

### Example: Getting Started Guide

```markdown
![ScratchBird Logo](images/logos/TransparentScratchBirdLogoHeader.png)

# Getting Started with ScratchBird

**Time to First Query:** < 5 minutes

This guide will help you...
```

✅ **Result:** Visually appealing introduction

---

### Example: Driver Documentation

```markdown
![ScratchBird Logo](images/logos/TransparentScratchBirdLogoHeader.png)

# Python Driver for ScratchBird

**Status:** Beta | **Version:** 0.9.0

Official Python driver implementing PEP 249...
```

✅ **Result:** Professional driver documentation

---

### Example: Installation Guide (More Subdued)

```markdown
# Installing ScratchBird on Linux

> ![ScratchBird](images/logos/ScratchBirdLogo.png){ width=48px }
> **Quick Install:** `sudo apt install scratchbird`

This guide covers installation...
```

✅ **Result:** Logo present but not overwhelming

---

## Quick Copy-Paste Templates

### Template 1: Full Header (Recommended)
```markdown
![ScratchBird Logo](images/logos/TransparentScratchBirdLogoHeader.png)

# [Page Title]

[Content...]
```

### Template 2: Small Header
```markdown
<img src="images/logos/TransparentScratchBirdLogoHeader.png" alt="ScratchBird" width="500">

# [Page Title]

[Content...]
```

### Template 3: Icon Only
```markdown
# [Page Title]

![ScratchBird](images/logos/ScratchBirdLogo.png){ width=64px }

[Content...]
```

### Template 4: No Logo (Simple Pages)
```markdown
# [Page Title]

[Content...]
```

**Use for:** Reference pages, troubleshooting, technical details

---

## Accessibility

Always include meaningful alt text:

```markdown
![ScratchBird Logo](images/logos/TransparentScratchBirdLogoHeader.png)
```

The `alt="ScratchBird Logo"` is automatically inferred from the text in `[]`.

For screen readers, this announces: "Image: ScratchBird Logo"

---

## Testing Logo Display

After adding a logo to a page:

1. **Preview locally** - Check markdown preview
2. **Test on GitHub** - View on actual GitHub wiki
3. **Check mobile** - Ensure scales properly
4. **Verify links** - Make sure image path is correct
5. **Dark mode** - Check visibility in dark mode (transparent version works best)

---

## Troubleshooting

### Logo Not Showing

**Problem:** Image doesn't display

**Solutions:**
1. Check file path is correct relative to document
2. Verify image file exists in `wiki/images/logos/`
3. Check file name capitalization (case-sensitive on Linux)
4. Ensure image was synced to wiki (run sync script)

### Logo Too Large

**Problem:** Logo takes up entire screen

**Solutions:**
1. Use HTML `<img>` tag with width attribute
2. Add CSS styling with max-width
3. Switch to smaller logo variant

### Logo Not Transparent

**Problem:** White/colored background visible

**Solutions:**
1. Use `TransparentScratchBirdLogoHeader.png` instead of `ScratchBirdLogoHeader.png`
2. Check if viewing in dark mode requires different logo

---

## Updating Logos

If logos are updated in `/docs/Artwork/`:

```bash
# Re-copy to wiki
cp docs/Artwork/TransparentScratchBirdLogoHeader.png wiki/images/logos/
cp docs/Artwork/ScratchBirdLogo.png wiki/images/logos/

# Sync to wiki
cd wiki/scripts
./sync-to-wiki.sh

# Or commit and let GitHub Actions sync
git add wiki/images/logos/
git commit -m "Update logos"
git push
```

---

## Summary

**Primary Logo:** `TransparentScratchBirdLogoHeader.png`
**Primary Use:** Headers on important wiki pages
**Placement:** Top of page, before H1 title
**Size:** Full width or ~600-800px
**Format:** Markdown image syntax

**Quick Markdown:**
```markdown
![ScratchBird Logo](images/logos/TransparentScratchBirdLogoHeader.png)
```

---

**See Also:**
- [Logo Files README](images/logos/README.md) - Technical details
- [Wiki Style Guide](templates/STYLE_GUIDE.md) - Writing standards
- [Contributing Guide](content/Contributing.md) - How to contribute

---

**Questions?** Ask in #documentation on Discord
