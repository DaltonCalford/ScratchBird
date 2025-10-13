#include "scratchbird/core/xml.h"
#include <iostream>
#include <cassert>

using namespace scratchbird::core;

int main() {
    std::cout << "Testing XML Implementation...\n\n";

    // Test 1: Simple element parsing
    std::cout << "Test 1: Simple element parsing\n";
    {
        auto root = XML::parse("<hello>world</hello>");
        assert(root.has_value());
        assert((*root)->name == "hello");
        assert((*root)->text == "world");
        assert((*root)->children.empty());
        std::cout << "  Simple element: " << (*root)->toXML() << " ✓\n";
    }
    std::cout << "  ✓ Simple element parsing passed\n\n";

    // Test 2: Self-closing tags
    std::cout << "Test 2: Self-closing tags\n";
    {
        auto root = XML::parse("<tag/>");
        assert(root.has_value());
        assert((*root)->name == "tag");
        assert((*root)->text.empty());
        std::cout << "  Self-closing: " << (*root)->toXML() << " ✓\n";

        auto root2 = XML::parse("<tag />");
        assert(root2.has_value());
        assert((*root2)->name == "tag");
        std::cout << "  Self-closing with space: " << (*root2)->toXML() << " ✓\n";
    }
    std::cout << "  ✓ Self-closing tags passed\n\n";

    // Test 3: Attributes
    std::cout << "Test 3: Attributes\n";
    {
        auto root = XML::parse("<book id=\"123\" title=\"Test Book\"/>");
        assert(root.has_value());
        assert((*root)->name == "book");

        auto id = (*root)->getAttribute("id");
        assert(id.has_value() && *id == "123");
        std::cout << "  Attribute id: " << *id << " ✓\n";

        auto title = (*root)->getAttribute("title");
        assert(title.has_value() && *title == "Test Book");
        std::cout << "  Attribute title: " << *title << " ✓\n";

        auto missing = (*root)->getAttribute("missing");
        assert(!missing.has_value());
        std::cout << "  Missing attribute: (not found) ✓\n";
    }
    std::cout << "  ✓ Attributes passed\n\n";

    // Test 4: Nested elements
    std::cout << "Test 4: Nested elements\n";
    {
        std::string xml = "<root><child1>text1</child1><child2>text2</child2></root>";
        auto root = XML::parse(xml);
        assert(root.has_value());
        assert((*root)->name == "root");
        assert((*root)->children.size() == 2);
        assert((*root)->children[0]->name == "child1");
        assert((*root)->children[0]->text == "text1");
        assert((*root)->children[1]->name == "child2");
        assert((*root)->children[1]->text == "text2");
        std::cout << "  Nested elements: " << (*root)->toXML(0);
    }
    std::cout << "  ✓ Nested elements passed\n\n";

    // Test 5: Entity encoding/decoding
    std::cout << "Test 5: Entity encoding/decoding\n";
    {
        auto root = XML::parse("<tag attr=\"&lt;test&gt;\">&amp;special&quot;chars&apos;</tag>");
        assert(root.has_value());

        auto attr = (*root)->getAttribute("attr");
        assert(attr.has_value() && *attr == "<test>");
        std::cout << "  Decoded attribute: " << *attr << " ✓\n";

        assert((*root)->text == "&special\"chars'");
        std::cout << "  Decoded text: " << (*root)->text << " ✓\n";

        std::string encoded = (*root)->toXML();
        std::cout << "  Re-encoded: " << encoded;
    }
    std::cout << "  ✓ Entity encoding/decoding passed\n\n";

    // Test 6: XML declaration
    std::cout << "Test 6: XML declaration handling\n";
    {
        std::string xml = "<?xml version=\"1.0\" encoding=\"UTF-8\"?><root>content</root>";
        auto root = XML::parse(xml);
        assert(root.has_value());
        assert((*root)->name == "root");
        assert((*root)->text == "content");
        std::cout << "  XML declaration skipped correctly ✓\n";
    }
    std::cout << "  ✓ XML declaration handling passed\n\n";

    // Test 7: Whitespace handling
    std::cout << "Test 7: Whitespace handling\n";
    {
        std::string xml = "  <root>  \n  <child>  text  </child>  \n  </root>  ";
        auto root = XML::parse(xml);
        assert(root.has_value());
        assert((*root)->name == "root");
        assert((*root)->children.size() == 1);
        assert((*root)->children[0]->text == "text"); // Whitespace trimmed
        std::cout << "  Whitespace trimmed correctly ✓\n";
    }
    std::cout << "  ✓ Whitespace handling passed\n\n";

    // Test 8: Find children by tag
    std::cout << "Test 8: Find children by tag\n";
    {
        std::string xml = "<root><item>1</item><item>2</item><other>3</other><item>4</item></root>";
        auto root = XML::parse(xml);
        assert(root.has_value());

        auto items = (*root)->findChildren("item");
        assert(items.size() == 3);
        assert(items[0]->text == "1");
        assert(items[1]->text == "2");
        assert(items[2]->text == "4");
        std::cout << "  Found " << items.size() << " 'item' elements ✓\n";

        auto others = (*root)->findChildren("other");
        assert(others.size() == 1);
        assert(others[0]->text == "3");
        std::cout << "  Found " << others.size() << " 'other' element ✓\n";

        auto missing = (*root)->findChildren("missing");
        assert(missing.empty());
        std::cout << "  Found " << missing.size() << " 'missing' elements ✓\n";
    }
    std::cout << "  ✓ Find children passed\n\n";

    // Test 9: XPath-like queries
    std::cout << "Test 9: XPath-like queries\n";
    {
        std::string xml = R"(
            <library>
                <book>
                    <title>Book 1</title>
                    <author>Author 1</author>
                </book>
                <book>
                    <title>Book 2</title>
                    <author>Author 2</author>
                </book>
            </library>
        )";

        auto root = XML::parse(xml);
        assert(root.has_value());

        // Query for titles
        auto titles = (*root)->query("book/title");
        assert(titles.size() == 2);
        assert(titles[0]->text == "Book 1");
        assert(titles[1]->text == "Book 2");
        std::cout << "  Query 'book/title' found " << titles.size() << " results ✓\n";

        // Query for authors
        auto authors = (*root)->query("book/author");
        assert(authors.size() == 2);
        assert(authors[0]->text == "Author 1");
        assert(authors[1]->text == "Author 2");
        std::cout << "  Query 'book/author' found " << authors.size() << " results ✓\n";
    }
    std::cout << "  ✓ XPath-like queries passed\n\n";

    // Test 10: Format/pretty print
    std::cout << "Test 10: Format/pretty print\n";
    {
        std::string xml = "<root><child1><grandchild>text</grandchild></child1></root>";
        std::string formatted = XML::format(xml);
        assert(!formatted.empty());
        std::cout << "  Formatted XML:\n" << formatted;
    }
    std::cout << "  ✓ Format/pretty print passed\n\n";

    // Test 11: Validation
    std::cout << "Test 11: XML validation\n";
    {
        assert(XML::validate("<valid>content</valid>"));
        std::cout << "  Valid XML accepted ✓\n";

        assert(!XML::validate("<invalid>"));
        std::cout << "  Unclosed tag rejected ✓\n";

        assert(!XML::validate("<open>text</close>"));
        std::cout << "  Mismatched tags rejected ✓\n";

        assert(!XML::validate("not xml"));
        std::cout << "  Non-XML rejected ✓\n";
    }
    std::cout << "  ✓ Validation passed\n\n";

    // Test 12: Complex real-world example
    std::cout << "Test 12: Real-world example\n";
    {
        std::string xml = R"(<?xml version="1.0"?>
            <bookstore>
                <book id="1" category="fiction">
                    <title lang="en">Harry Potter</title>
                    <author>J.K. Rowling</author>
                    <year>2005</year>
                    <price>29.99</price>
                </book>
                <book id="2" category="tech">
                    <title lang="en">Learning XML</title>
                    <author>Erik T. Ray</author>
                    <year>2003</year>
                    <price>39.95</price>
                </book>
            </bookstore>
        )";

        auto root = XML::parse(xml);
        assert(root.has_value());
        assert((*root)->name == "bookstore");
        std::cout << "  Parsed complex bookstore XML ✓\n";

        auto books = (*root)->findChildren("book");
        assert(books.size() == 2);
        std::cout << "  Found " << books.size() << " books ✓\n";

        auto id1 = books[0]->getAttribute("id");
        assert(id1.has_value() && *id1 == "1");
        std::cout << "  Book 1 id: " << *id1 << " ✓\n";

        auto category1 = books[0]->getAttribute("category");
        assert(category1.has_value() && *category1 == "fiction");
        std::cout << "  Book 1 category: " << *category1 << " ✓\n";

        auto titles = (*root)->query("book/title");
        assert(titles.size() == 2);
        assert(titles[0]->text == "Harry Potter");
        assert(titles[1]->text == "Learning XML");
        std::cout << "  Titles: " << titles[0]->text << ", " << titles[1]->text << " ✓\n";

        auto prices = (*root)->query("book/price");
        assert(prices.size() == 2);
        assert(prices[0]->text == "29.99");
        assert(prices[1]->text == "39.95");
        std::cout << "  Prices: $" << prices[0]->text << ", $" << prices[1]->text << " ✓\n";
    }
    std::cout << "  ✓ Real-world example passed\n\n";

    // Test 13: setAttribute and addChild methods
    std::cout << "Test 13: Building XML programmatically\n";
    {
        auto root = std::make_shared<XMLNode>("person");
        root->setAttribute("id", "123");
        root->setAttribute("name", "John Doe");

        auto address = std::make_shared<XMLNode>("address");
        address->text = "123 Main St";
        root->addChild(address);

        auto phone = std::make_shared<XMLNode>("phone");
        phone->text = "555-1234";
        root->addChild(phone);

        assert(root->getAttribute("id").value() == "123");
        assert(root->getAttribute("name").value() == "John Doe");
        assert(root->children.size() == 2);
        assert(root->children[0]->text == "123 Main St");
        assert(root->children[1]->text == "555-1234");

        std::cout << "  Programmatically built XML:\n" << root->toXML(0);
    }
    std::cout << "  ✓ Building XML programmatically passed\n\n";

    // Test 14: Empty elements
    std::cout << "Test 14: Empty elements\n";
    {
        auto root1 = XML::parse("<empty/>");
        assert(root1.has_value());
        assert((*root1)->text.empty());
        assert((*root1)->children.empty());
        std::cout << "  Self-closing empty: " << (*root1)->toXML();

        auto root2 = XML::parse("<empty></empty>");
        assert(root2.has_value());
        assert((*root2)->text.empty());
        assert((*root2)->children.empty());
        std::cout << "  Open/close empty: " << (*root2)->toXML();
    }
    std::cout << "  ✓ Empty elements passed\n\n";

    std::cout << "========================================\n";
    std::cout << "ALL TESTS PASSED! ✓\n";
    std::cout << "XML type is fully functional.\n";
    std::cout << "========================================\n";

    return 0;
}
