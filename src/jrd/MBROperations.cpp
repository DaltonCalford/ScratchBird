#include "MBROperations.h"
#include <sstream>
#include <iomanip>
#include <cmath>
#include <algorithm>
#include <numeric>

using namespace ScratchBird;

//============================================================================
// ExtendedMBR Implementation
//============================================================================

bool ExtendedMBR::strictlyContains(const ExtendedMBR& other) const
{
    return (other.minX > minX && other.maxX < maxX &&
            other.minY > minY && other.maxY < maxY);
}

bool ExtendedMBR::touchesBoundary(const ExtendedMBR& other) const
{
    return intersects(other) && !strictlyContains(other) && !other.strictlyContains(*this);
}

bool ExtendedMBR::overlapsArea(const ExtendedMBR& other) const
{
    return intersects(other) && !contains(other) && !other.contains(*this);
}

double ExtendedMBR::overlapArea(const ExtendedMBR& other) const
{
    if (!intersects(other)) return 0.0;
    
    double overlapMinX = std::max(minX, other.minX);
    double overlapMinY = std::max(minY, other.minY);
    double overlapMaxX = std::min(maxX, other.maxX);
    double overlapMaxY = std::min(maxY, other.maxY);
    
    return (overlapMaxX - overlapMinX) * (overlapMaxY - overlapMinY);
}

double ExtendedMBR::intersectionArea(const ExtendedMBR& other) const
{
    return overlapArea(other);
}

double ExtendedMBR::distance(const ExtendedMBR& other) const
{
    return minDistance(other);
}

double ExtendedMBR::minDistance(const ExtendedMBR& other) const
{
    if (intersects(other)) return 0.0;
    
    double dx = 0.0;
    if (other.maxX < minX) {
        dx = minX - other.maxX;
    } else if (other.minX > maxX) {
        dx = other.minX - maxX;
    }
    
    double dy = 0.0;
    if (other.maxY < minY) {
        dy = minY - other.maxY;
    } else if (other.minY > maxY) {
        dy = other.minY - maxY;
    }
    
    return std::sqrt(dx * dx + dy * dy);
}

double ExtendedMBR::maxDistance(const ExtendedMBR& other) const
{
    // Maximum distance between any two points in the MBRs
    double dx = std::max(std::abs(minX - other.maxX), std::abs(maxX - other.minX));
    double dy = std::max(std::abs(minY - other.maxY), std::abs(maxY - other.minY));
    
    return std::sqrt(dx * dx + dy * dy);
}

double ExtendedMBR::centerDistance(const ExtendedMBR& other) const
{
    Coordinate center1 = getCenter();
    Coordinate center2 = other.getCenter();
    
    return center1.distance2D(center2);
}

bool ExtendedMBR::isWithinDistance(const ExtendedMBR& other, double maxDistance) const
{
    return minDistance(other) <= maxDistance;
}

Coordinate ExtendedMBR::getCenter() const
{
    return Coordinate((minX + maxX) / 2.0, (minY + maxY) / 2.0);
}

double ExtendedMBR::getDiagonalLength() const
{
    double dx = maxX - minX;
    double dy = maxY - minY;
    return std::sqrt(dx * dx + dy * dy);
}

double ExtendedMBR::getAspectRatio() const
{
    double width = getWidth();
    double height = getHeight();
    
    if (height == 0.0) return INFINITY;
    return width / height;
}

void ExtendedMBR::expandByDistance(double distance)
{
    minX -= distance;
    minY -= distance;
    maxX += distance;
    maxY += distance;
}

void ExtendedMBR::expandByPercentage(double percentage)
{
    double centerX = (minX + maxX) / 2.0;
    double centerY = (minY + maxY) / 2.0;
    double halfWidth = (maxX - minX) / 2.0;
    double halfHeight = (maxY - minY) / 2.0;
    
    double factor = 1.0 + percentage / 100.0;
    halfWidth *= factor;
    halfHeight *= factor;
    
    minX = centerX - halfWidth;
    maxX = centerX + halfWidth;
    minY = centerY - halfHeight;
    maxY = centerY + halfHeight;
}

void ExtendedMBR::expandToInclude(const Coordinate& coord)
{
    expand(coord.x, coord.y);
}

void ExtendedMBR::expandToInclude(const ExtendedMBR& other)
{
    expand(other);
}

ExtendedMBR ExtendedMBR::getExpanded(double distance) const
{
    ExtendedMBR result = *this;
    result.expandByDistance(distance);
    return result;
}

ExtendedMBR ExtendedMBR::getExpanded(double xDistance, double yDistance) const
{
    ExtendedMBR result = *this;
    result.minX -= xDistance;
    result.maxX += xDistance;
    result.minY -= yDistance;
    result.maxY += yDistance;
    return result;
}

std::pair<ExtendedMBR, ExtendedMBR> ExtendedMBR::splitHorizontal(double ratio) const
{
    double splitY = minY + (maxY - minY) * ratio;
    
    ExtendedMBR bottom(minX, minY, maxX, splitY);
    ExtendedMBR top(minX, splitY, maxX, maxY);
    
    return std::make_pair(bottom, top);
}

std::pair<ExtendedMBR, ExtendedMBR> ExtendedMBR::splitVertical(double ratio) const
{
    double splitX = minX + (maxX - minX) * ratio;
    
    ExtendedMBR left(minX, minY, splitX, maxY);
    ExtendedMBR right(splitX, minY, maxX, maxY);
    
    return std::make_pair(left, right);
}

std::vector<ExtendedMBR> ExtendedMBR::subdivide(USHORT numX, USHORT numY) const
{
    std::vector<ExtendedMBR> subdivisions;
    subdivisions.reserve(numX * numY);
    
    double stepX = getWidth() / numX;
    double stepY = getHeight() / numY;
    
    for (USHORT y = 0; y < numY; y++) {
        for (USHORT x = 0; x < numX; x++) {
            double left = minX + x * stepX;
            double bottom = minY + y * stepY;
            double right = left + stepX;
            double top = bottom + stepY;
            
            subdivisions.emplace_back(left, bottom, right, top);
        }
    }
    
    return subdivisions;
}

double ExtendedMBR::getDeadSpace(const std::vector<ExtendedMBR>& children) const
{
    double totalChildArea = 0.0;
    for (const ExtendedMBR& child : children) {
        totalChildArea += child.area();
    }
    
    return area() - totalChildArea;
}

double ExtendedMBR::getCoverage(const std::vector<ExtendedMBR>& children) const
{
    if (area() == 0.0) return 1.0;
    
    double totalChildArea = 0.0;
    for (const ExtendedMBR& child : children) {
        totalChildArea += child.area();
    }
    
    return totalChildArea / area();
}

double ExtendedMBR::getOverlap(const std::vector<ExtendedMBR>& siblings) const
{
    double totalOverlap = 0.0;
    
    for (const ExtendedMBR& sibling : siblings) {
        if (&sibling != this) {
            totalOverlap += overlapArea(sibling);
        }
    }
    
    return totalOverlap;
}

bool ExtendedMBR::isNormalized() const
{
    return minX <= maxX && minY <= maxY;
}

void ExtendedMBR::normalize()
{
    if (minX > maxX) std::swap(minX, maxX);
    if (minY > maxY) std::swap(minY, maxY);
}

ExtendedMBR ExtendedMBR::getNormalized() const
{
    ExtendedMBR result = *this;
    result.normalize();
    return result;
}

bool ExtendedMBR::equals(const ExtendedMBR& other, double tolerance) const
{
    return (std::abs(minX - other.minX) <= tolerance &&
            std::abs(minY - other.minY) <= tolerance &&
            std::abs(maxX - other.maxX) <= tolerance &&
            std::abs(maxY - other.maxY) <= tolerance);
}

int ExtendedMBR::compareByArea(const ExtendedMBR& other) const
{
    double thisArea = area();
    double otherArea = other.area();
    
    if (thisArea < otherArea) return -1;
    if (thisArea > otherArea) return 1;
    return 0;
}

int ExtendedMBR::compareByCenter(const ExtendedMBR& other) const
{
    Coordinate thisCenter = getCenter();
    Coordinate otherCenter = other.getCenter();
    
    if (thisCenter.x < otherCenter.x) return -1;
    if (thisCenter.x > otherCenter.x) return 1;
    if (thisCenter.y < otherCenter.y) return -1;
    if (thisCenter.y > otherCenter.y) return 1;
    return 0;
}

string ExtendedMBR::toString() const
{
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(6);
    oss << "MBR(" << minX << "," << minY << "," << maxX << "," << maxY << ")";
    return oss.str();
}

ExtendedMBR ExtendedMBR::fromString(const string& str)
{
    // Parse "MBR(minX,minY,maxX,maxY)" format
    // Simplified implementation - would need more robust parsing in production
    size_t start = str.find('(');
    size_t end = str.find(')');
    
    if (start == string::npos || end == string::npos) {
        return ExtendedMBR(); // Return invalid MBR
    }
    
    string coords = str.substr(start + 1, end - start - 1);
    // Parse coordinates...
    return ExtendedMBR(); // Simplified - return invalid MBR
}

bool ExtendedMBR::operator==(const ExtendedMBR& other) const
{
    return equals(other);
}

bool ExtendedMBR::operator!=(const ExtendedMBR& other) const
{
    return !equals(other);
}

ExtendedMBR ExtendedMBR::operator+(const ExtendedMBR& other) const
{
    ExtendedMBR result = *this;
    result.expand(other);
    return result;
}

ExtendedMBR ExtendedMBR::operator*(const ExtendedMBR& other) const
{
    if (!intersects(other)) {
        return ExtendedMBR(); // Empty intersection
    }
    
    double intersectMinX = std::max(minX, other.minX);
    double intersectMinY = std::max(minY, other.minY);
    double intersectMaxX = std::min(maxX, other.maxX);
    double intersectMaxY = std::min(maxY, other.maxY);
    
    return ExtendedMBR(intersectMinX, intersectMinY, intersectMaxX, intersectMaxY);
}

//============================================================================
// MBRCollection Implementation
//============================================================================

MBRCollection::MBRCollection() : boundingMBRValid(false)
{
}

MBRCollection::~MBRCollection()
{
}

void MBRCollection::add(const ExtendedMBR& mbr)
{
    mbrs.push_back(mbr);
    invalidateBoundingMBR();
}

void MBRCollection::remove(ULONG index)
{
    if (index < mbrs.size()) {
        mbrs.erase(mbrs.begin() + index);
        invalidateBoundingMBR();
    }
}

void MBRCollection::clear()
{
    mbrs.clear();
    invalidateBoundingMBR();
}

const ExtendedMBR& MBRCollection::operator[](ULONG index) const
{
    return mbrs[index];
}

ExtendedMBR& MBRCollection::operator[](ULONG index)
{
    invalidateBoundingMBR(); // Might be modified
    return mbrs[index];
}

ExtendedMBR MBRCollection::getBoundingMBR()
{
    if (!boundingMBRValid) {
        updateBoundingMBR();
    }
    return boundingMBR;
}

void MBRCollection::updateBoundingMBR()
{
    boundingMBR.reset();
    
    for (const ExtendedMBR& mbr : mbrs) {
        boundingMBR.expand(mbr);
    }
    
    boundingMBRValid = true;
}

double MBRCollection::getTotalArea() const
{
    double total = 0.0;
    for (const ExtendedMBR& mbr : mbrs) {
        total += mbr.area();
    }
    return total;
}

double MBRCollection::getTotalOverlap() const
{
    double total = 0.0;
    
    for (size_t i = 0; i < mbrs.size(); i++) {
        for (size_t j = i + 1; j < mbrs.size(); j++) {
            total += mbrs[i].overlapArea(mbrs[j]);
        }
    }
    
    return total;
}

double MBRCollection::getAverageArea() const
{
    if (mbrs.empty()) return 0.0;
    return getTotalArea() / mbrs.size();
}

Coordinate MBRCollection::getCentroid() const
{
    if (mbrs.empty()) return Coordinate(0, 0);
    
    double totalX = 0.0, totalY = 0.0;
    
    for (const ExtendedMBR& mbr : mbrs) {
        Coordinate center = mbr.getCenter();
        totalX += center.x;
        totalY += center.y;
    }
    
    return Coordinate(totalX / mbrs.size(), totalY / mbrs.size());
}

std::vector<ULONG> MBRCollection::findIntersecting(const ExtendedMBR& queryMBR) const
{
    std::vector<ULONG> results;
    
    for (ULONG i = 0; i < mbrs.size(); i++) {
        if (mbrs[i].intersects(queryMBR)) {
            results.push_back(i);
        }
    }
    
    return results;
}

std::vector<ULONG> MBRCollection::findContained(const ExtendedMBR& queryMBR) const
{
    std::vector<ULONG> results;
    
    for (ULONG i = 0; i < mbrs.size(); i++) {
        if (queryMBR.contains(mbrs[i])) {
            results.push_back(i);
        }
    }
    
    return results;
}

std::vector<ULONG> MBRCollection::findContaining(const ExtendedMBR& queryMBR) const
{
    std::vector<ULONG> results;
    
    for (ULONG i = 0; i < mbrs.size(); i++) {
        if (mbrs[i].contains(queryMBR)) {
            results.push_back(i);
        }
    }
    
    return results;
}

std::vector<ULONG> MBRCollection::findWithinDistance(const ExtendedMBR& queryMBR, double distance) const
{
    std::vector<ULONG> results;
    
    for (ULONG i = 0; i < mbrs.size(); i++) {
        if (mbrs[i].isWithinDistance(queryMBR, distance)) {
            results.push_back(i);
        }
    }
    
    return results;
}

void MBRCollection::sortByArea(bool ascending)
{
    std::sort(mbrs.begin(), mbrs.end(), [ascending](const ExtendedMBR& a, const ExtendedMBR& b) {
        return ascending ? (a.area() < b.area()) : (a.area() > b.area());
    });
}

void MBRCollection::sortByX(bool ascending)
{
    std::sort(mbrs.begin(), mbrs.end(), [ascending](const ExtendedMBR& a, const ExtendedMBR& b) {
        return ascending ? (a.minX < b.minX) : (a.minX > b.minX);
    });
}

void MBRCollection::sortByY(bool ascending)
{
    std::sort(mbrs.begin(), mbrs.end(), [ascending](const ExtendedMBR& a, const ExtendedMBR& b) {
        return ascending ? (a.minY < b.minY) : (a.minY > b.minY);
    });
}

void MBRCollection::sortByCenterDistance(const Coordinate& center, bool ascending)
{
    std::sort(mbrs.begin(), mbrs.end(), [&center, ascending](const ExtendedMBR& a, const ExtendedMBR& b) {
        double distA = a.getCenter().distance2D(center);
        double distB = b.getCenter().distance2D(center);
        return ascending ? (distA < distB) : (distA > distB);
    });
}

void MBRCollection::sortByOverlap(const ExtendedMBR& reference, bool ascending)
{
    std::sort(mbrs.begin(), mbrs.end(), [&reference, ascending](const ExtendedMBR& a, const ExtendedMBR& b) {
        double overlapA = a.overlapArea(reference);
        double overlapB = b.overlapArea(reference);
        return ascending ? (overlapA < overlapB) : (overlapA > overlapB);
    });
}

//============================================================================
// MBRAlgorithms Implementation
//============================================================================

namespace MBRAlgorithms
{
    std::pair<MBRCollection, MBRCollection> linearSplit(const MBRCollection& mbrs)
    {
        // Simplified linear split implementation
        MBRCollection group1, group2;
        
        if (mbrs.size() < 2) {
            group1 = mbrs;
            return std::make_pair(group1, group2);
        }
        
        // Pick seeds using linear method
        std::pair<ULONG, ULONG> seeds = pickLinearSeeds(mbrs);
        
        group1.add(mbrs[seeds.first]);
        group2.add(mbrs[seeds.second]);
        
        // Assign remaining MBRs
        for (ULONG i = 0; i < mbrs.size(); i++) {
            if (i != seeds.first && i != seeds.second) {
                ExtendedMBR mbr1 = group1.getBoundingMBR();
                ExtendedMBR mbr2 = group2.getBoundingMBR();
                
                double enlargement1 = mbr1.enlargement(mbrs[i]);
                double enlargement2 = mbr2.enlargement(mbrs[i]);
                
                if (enlargement1 < enlargement2) {
                    group1.add(mbrs[i]);
                } else {
                    group2.add(mbrs[i]);
                }
            }
        }
        
        return std::make_pair(group1, group2);
    }
    
    std::pair<ULONG, ULONG> pickLinearSeeds(const MBRCollection& mbrs)
    {
        if (mbrs.size() < 2) return std::make_pair(0, 0);
        
        // Find the pair with maximum separation along one axis
        double maxSeparation = 0.0;
        ULONG seed1 = 0, seed2 = 1;
        
        // Check X-axis separation
        for (ULONG i = 0; i < mbrs.size(); i++) {
            for (ULONG j = i + 1; j < mbrs.size(); j++) {
                double separation = std::abs(mbrs[i].getCenter().x - mbrs[j].getCenter().x);
                if (separation > maxSeparation) {
                    maxSeparation = separation;
                    seed1 = i;
                    seed2 = j;
                }
            }
        }
        
        // Check Y-axis separation
        for (ULONG i = 0; i < mbrs.size(); i++) {
            for (ULONG j = i + 1; j < mbrs.size(); j++) {
                double separation = std::abs(mbrs[i].getCenter().y - mbrs[j].getCenter().y);
                if (separation > maxSeparation) {
                    maxSeparation = separation;
                    seed1 = i;
                    seed2 = j;
                }
            }
        }
        
        return std::make_pair(seed1, seed2);
    }
    
    ULONG chooseLeaf(const MBRCollection& candidates, const ExtendedMBR& newMBR, OptimizationStrategy strategy)
    {
        if (candidates.size() == 0) return 0;
        if (candidates.size() == 1) return 0;
        
        ULONG bestIndex = 0;
        double bestValue = INFINITY;
        
        switch (strategy) {
            case OPT_MINIMIZE_AREA: {
                for (ULONG i = 0; i < candidates.size(); i++) {
                    double enlargement = candidates[i].enlargement(newMBR);
                    if (enlargement < bestValue) {
                        bestValue = enlargement;
                        bestIndex = i;
                    }
                }
                break;
            }
            
            case OPT_MINIMIZE_OVERLAP: {
                for (ULONG i = 0; i < candidates.size(); i++) {
                    ExtendedMBR expanded = candidates[i] + newMBR;
                    double totalOverlap = 0.0;
                    
                    for (ULONG j = 0; j < candidates.size(); j++) {
                        if (i != j) {
                            totalOverlap += expanded.overlapArea(candidates[j]);
                        }
                    }
                    
                    if (totalOverlap < bestValue) {
                        bestValue = totalOverlap;
                        bestIndex = i;
                    }
                }
                break;
            }
            
            case OPT_MINIMIZE_MARGIN: {
                for (ULONG i = 0; i < candidates.size(); i++) {
                    ExtendedMBR expanded = candidates[i] + newMBR;
                    double margin = expanded.getMargin();
                    
                    if (margin < bestValue) {
                        bestValue = margin;
                        bestIndex = i;
                    }
                }
                break;
            }
            
            default:
                // Use area enlargement as default
                for (ULONG i = 0; i < candidates.size(); i++) {
                    double enlargement = candidates[i].enlargement(newMBR);
                    if (enlargement < bestValue) {
                        bestValue = enlargement;
                        bestIndex = i;
                    }
                }
                break;
        }
        
        return bestIndex;
    }
    
    double calculateDeadSpace(const ExtendedMBR& container, const MBRCollection& children)
    {
        return container.getDeadSpace(children.getMBRs());
    }
    
    double calculateOverlap(const MBRCollection& mbrs)
    {
        return mbrs.getTotalOverlap();
    }
    
    double calculateMargin(const MBRCollection& mbrs)
    {
        double totalMargin = 0.0;
        
        for (ULONG i = 0; i < mbrs.size(); i++) {
            totalMargin += mbrs[i].getMargin();
        }
        
        return totalMargin;
    }
    
    std::vector<ULONG> findKNearestNeighbors(const MBRCollection& mbrs, const ExtendedMBR& query, ULONG k)
    {
        std::vector<std::pair<double, ULONG>> distances;
        
        for (ULONG i = 0; i < mbrs.size(); i++) {
            double distance = mbrs[i].minDistance(query);
            distances.emplace_back(distance, i);
        }
        
        // Sort by distance
        std::sort(distances.begin(), distances.end());
        
        // Extract indices of k nearest
        std::vector<ULONG> result;
        for (ULONG i = 0; i < std::min(k, (ULONG)distances.size()); i++) {
            result.push_back(distances[i].second);
        }
        
        return result;
    }
}

//============================================================================
// MBRStatistics Implementation
//============================================================================

double MBRStatistics::getMinArea() const
{
    if (collection.size() == 0) return 0.0;
    
    double minArea = INFINITY;
    for (ULONG i = 0; i < collection.size(); i++) {
        double area = collection[i].area();
        if (area < minArea) {
            minArea = area;
        }
    }
    
    return minArea;
}

double MBRStatistics::getMaxArea() const
{
    if (collection.size() == 0) return 0.0;
    
    double maxArea = 0.0;
    for (ULONG i = 0; i < collection.size(); i++) {
        double area = collection[i].area();
        if (area > maxArea) {
            maxArea = area;
        }
    }
    
    return maxArea;
}

double MBRStatistics::getAverageArea() const
{
    return collection.getAverageArea();
}

double MBRStatistics::getMedianArea() const
{
    if (collection.size() == 0) return 0.0;
    
    std::vector<double> areas;
    for (ULONG i = 0; i < collection.size(); i++) {
        areas.push_back(collection[i].area());
    }
    
    std::sort(areas.begin(), areas.end());
    
    size_t n = areas.size();
    if (n % 2 == 0) {
        return (areas[n/2 - 1] + areas[n/2]) / 2.0;
    } else {
        return areas[n/2];
    }
}

double MBRStatistics::getAreaStandardDeviation() const
{
    if (collection.size() <= 1) return 0.0;
    
    double mean = getAverageArea();
    double sumSquaredDiffs = 0.0;
    
    for (ULONG i = 0; i < collection.size(); i++) {
        double diff = collection[i].area() - mean;
        sumSquaredDiffs += diff * diff;
    }
    
    return std::sqrt(sumSquaredDiffs / (collection.size() - 1));
}

string MBRStatistics::generateReport() const
{
    std::ostringstream report;
    
    report << "MBR Collection Statistics Report\n";
    report << "================================\n";
    report << "Total MBRs: " << collection.size() << "\n";
    report << "Total Area: " << std::fixed << std::setprecision(2) << collection.getTotalArea() << "\n";
    report << "Average Area: " << getAverageArea() << "\n";
    report << "Minimum Area: " << getMinArea() << "\n";
    report << "Maximum Area: " << getMaxArea() << "\n";
    report << "Median Area: " << getMedianArea() << "\n";
    report << "Area Std Dev: " << getAreaStandardDeviation() << "\n";
    report << "Total Overlap: " << collection.getTotalOverlap() << "\n";
    report << "Average Overlap: " << getAverageOverlap() << "\n";
    
    return report.str();
}

double MBRStatistics::getAverageOverlap() const
{
    if (collection.size() <= 1) return 0.0;
    
    double totalOverlap = collection.getTotalOverlap();
    ULONG pairCount = collection.size() * (collection.size() - 1) / 2;
    
    return totalOverlap / pairCount;
}

//============================================================================
// MBRValidation Implementation  
//============================================================================

namespace MBRValidation
{
    bool isValidMBR(const ExtendedMBR& mbr)
    {
        return mbr.isValid() && mbr.isNormalized();
    }
    
    bool isValidCollection(const MBRCollection& collection)
    {
        for (ULONG i = 0; i < collection.size(); i++) {
            if (!isValidMBR(collection[i])) {
                return false;
            }
        }
        return true;
    }
    
    std::vector<string> validateMBR(const ExtendedMBR& mbr)
    {
        std::vector<string> errors;
        
        if (!mbr.isValid()) {
            errors.push_back("Invalid MBR: coordinates out of range or malformed");
        }
        
        if (!mbr.isNormalized()) {
            errors.push_back("MBR not normalized: min values greater than max values");
        }
        
        if (mbr.area() < 0) {
            errors.push_back("MBR has negative area");
        }
        
        return errors;
    }
    
    ExtendedMBR repairMBR(const ExtendedMBR& mbr)
    {
        ExtendedMBR repaired = mbr;
        repaired.normalize();
        
        // Clamp coordinates to valid range
        repaired.minX = std::max(repaired.minX, MIN_COORDINATE);
        repaired.minY = std::max(repaired.minY, MIN_COORDINATE);
        repaired.maxX = std::min(repaired.maxX, MAX_COORDINATE);
        repaired.maxY = std::min(repaired.maxY, MAX_COORDINATE);
        
        return repaired;
    }
    
    double assessQuality(const MBRCollection& collection)
    {
        if (collection.size() == 0) return 1.0;
        
        double qualityScore = 1.0;
        
        // Penalize for overlaps
        double totalOverlap = collection.getTotalOverlap();
        double totalArea = collection.getTotalArea();
        if (totalArea > 0) {
            double overlapRatio = totalOverlap / totalArea;
            qualityScore -= overlapRatio * 0.5; // Up to 50% penalty for overlaps
        }
        
        // Penalize for area variance (prefer uniform sizes)
        MBRStatistics stats(collection);
        double avgArea = stats.getAverageArea();
        double stdDev = stats.getAreaStandardDeviation();
        if (avgArea > 0) {
            double varianceRatio = stdDev / avgArea;
            qualityScore -= varianceRatio * 0.3; // Up to 30% penalty for high variance
        }
        
        return std::max(0.0, qualityScore);
    }
}

//============================================================================
// MBROptimization Implementation
//============================================================================

namespace MBROptimization
{
    std::vector<bool> batchIntersects(const ExtendedMBR& query, const MBRCollection& collection)
    {
        std::vector<bool> results;
        results.reserve(collection.size());
        
        for (ULONG i = 0; i < collection.size(); i++) {
            results.push_back(collection[i].intersects(query));
        }
        
        return results;
    }
    
    std::vector<bool> batchContains(const ExtendedMBR& query, const MBRCollection& collection)
    {
        std::vector<bool> results;
        results.reserve(collection.size());
        
        for (ULONG i = 0; i < collection.size(); i++) {
            results.push_back(query.contains(collection[i]));
        }
        
        return results;
    }
    
    std::vector<double> batchDistances(const ExtendedMBR& query, const MBRCollection& collection)
    {
        std::vector<double> results;
        results.reserve(collection.size());
        
        for (ULONG i = 0; i < collection.size(); i++) {
            results.push_back(collection[i].minDistance(query));
        }
        
        return results;
    }
    
    ULONG estimateMemoryUsage(const MBRCollection& collection)
    {
        // Estimate memory usage in bytes
        ULONG baseSize = sizeof(MBRCollection);
        ULONG mbrSize = collection.size() * sizeof(ExtendedMBR);
        ULONG vectorOverhead = collection.size() * sizeof(void*); // Approximate vector overhead
        
        return baseSize + mbrSize + vectorOverhead;
    }
}

//============================================================================
// MBRCache Implementation (in MBROptimization namespace)
//============================================================================

MBROptimization::MBRCache::MBRCache(ULONG size) : maxSize(size), accessCounter(0)
{
    cache.reserve(maxSize);
}

MBROptimization::MBRCache::~MBRCache()
{
}

bool MBROptimization::MBRCache::get(ULONG id, ExtendedMBR& mbr)
{
    for (auto& entry : cache) {
        if (entry.lastAccess == id) { // Using lastAccess as ID for simplicity
            mbr = entry.mbr;
            entry.lastAccess = ++accessCounter;
            return true;
        }
    }
    return false;
}

void MBROptimization::MBRCache::put(ULONG id, const ExtendedMBR& mbr)
{
    if (cache.size() >= maxSize) {
        evictOldest();
    }
    
    CacheEntry entry;
    entry.mbr = mbr;
    entry.lastAccess = ++accessCounter;
    entry.dirty = false;
    
    cache.push_back(entry);
}

void MBROptimization::MBRCache::evictOldest()
{
    if (cache.empty()) return;
    
    auto oldest = std::min_element(cache.begin(), cache.end(),
        [](const CacheEntry& a, const CacheEntry& b) {
            return a.lastAccess < b.lastAccess;
        });
    
    cache.erase(oldest);
}