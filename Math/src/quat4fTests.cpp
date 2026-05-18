#include "Quat4f.h"
#include <iostream>
#include <cassert>
#include <cmath>

// Assuming your existing headers are included here:
// #include "Quat4f.h"
// #include "Vec3.h"
// #include "Matrix3f.h"

// Floating-point helper for loose precision testing
bool IsClose(float a, float b, float epsilon = 1e-4f) {
    return std::fabs(a - b) < epsilon;
}

// Helper function converting Quat4f back to Matrix3f (using column-major or row-major standard)
Matrix3f QuatToMatrix(const Quat4f& q) {
    Matrix3f m;
    float w = q.w(), x = q.x(), y = q.y(), z = q.z();
    
    // Assuming a standard row/col layout. Adjust indices m(row, col) to match your Matrix3f layout.
    m(0, 0) = 1.0f - 2.0f * (y * y + z * z);
    m(0, 1) = 2.0f * (x * y - w * z);
    m(0, 2) = 2.0f * (x * z + w * y);

    m(1, 0) = 2.0f * (x * y + w * z);
    m(1, 1) = 1.0f - 2.0f * (x * x + z * z);
    m(1, 2) = 2.0f * (y * z - w * x);

    m(2, 0) = 2.0f * (x * z - w * y);
    m(2, 1) = 2.0f * (y * z + w * x);
    m(2, 2) = 1.0f - 2.0f * (x * x + y * y);
    
    return m;
}

// ==========================================================================
// TEST 1: Identity Edge Case (Verifies the division-by-zero fix)
// ==========================================================================
void TestIdentityConversion() {
    Quat4f qIdent(1.0f, 0.0f, 0.0f, 0.0f); // Or Quat4f::IDENTITY if updated
    
    // 1. Test Matrix Conversion
    Matrix3f mat = QuatToMatrix(qIdent);
    assert(IsClose(mat(0,0), 1.0f) && IsClose(mat(1,1), 1.0f) && IsClose(mat(2,2), 1.0f));
    assert(IsClose(mat(0,1), 0.0f) && IsClose(mat(0,2), 0.0f) && IsClose(mat(1,2), 0.0f));

    // 2. Test Axis-Angle Conversion (Triggers the vectorNorm < 1e-6 safety track)
    float radians = -1.0f;
    Vec3f axis = qIdent.getAxisAngle(&radians);
    
    assert(IsClose(radians, 0.0f));
    // The length of the returned axis vector should still be a valid unit vector (e.g., [1,0,0])
    assert(IsClose(axis.norm(), 1.0f)); 
    
    std::cout << "[PASS] Identity Edge Case Tests Passed.\n";
}

// ==========================================================================
// TEST 2: 90-Degree Rotation Around X-Axis
// ==========================================================================
void TestRotationX90() {
    // 90 degrees around X axis: half angle is 45 degrees
    float radiansInput = static_cast<float>(M_PI / 2.0); // 90 deg
    Vec3f axisInput(1.0f, 0.0f, 0.0f);
    
    Quat4f q;
    q.setAxisAngle(radiansInput, axisInput);
    
    // Expected Quaternion components: [cos(45), sin(45), 0, 0] -> [0.7071, 0.7071, 0, 0]
    assert(IsClose(q.w(), 0.707107f));
    assert(IsClose(q.x(), 0.707107f));
    assert(IsClose(q.y(), 0.0f));
    assert(IsClose(q.z(), 0.0f));

    // 1. Verify Matrix output
    Matrix3f mat = QuatToMatrix(q);
    // Rotating Y vector [0, 1, 0] by 90 deg around X should yield Z vector [0, 0, 1]
    // (Depending on your system's handness, this checks standard rotation row mapping)
    assert(IsClose(mat(0,0), 1.0f));
    assert(IsClose(mat(1,1), 0.0f));
    assert(IsClose(mat(2,1), 1.0f));  // Y rolls into Z
    assert(IsClose(mat(1,2), -1.0f)); // Z rolls into -Y

    // 2. Verify round-trip Axis Angle recovery
    float radiansOutput = 0.0f;
    Vec3f axisOutput = q.getAxisAngle(&radiansOutput);
    
    assert(IsClose(radiansOutput, radiansInput));
    assert(IsClose(axisOutput[0], 1.0f));
    assert(IsClose(axisOutput[1], 0.0f));
    assert(IsClose(axisOutput[2], 0.0f));

    std::cout << "[PASS] 90-Degree X-Axis Roundtrip Tests Passed.\n";
}

// ==========================================================================
// TEST 3: Matrix Matrix Round-Trip (FromMatrix -> ToMatrix)
// ==========================================================================
void TestMatrixRoundTrip() {
    // Manually create a 180-degree rotation matrix around Y axis
    // [ -1  0  0 ]
    // [  0  1  0 ]
    // [  0  0 -1 ]
    Matrix3f initialMat;
    initialMat(0,0) = -1.0f; initialMat(0,1) =  0.0f; initialMat(0,2) =  0.0f;
    initialMat(1,0) =  0.0f; initialMat(1,1) =  1.0f; initialMat(1,2) =  0.0f;
    initialMat(2,0) =  0.0f; initialMat(2,1) =  0.0f; initialMat(2,2) = -1.0f;

    // Convert Matrix -> Quaternion
    Quat4f q = Quat4f::fromRotationMatrix(initialMat);
    
    // Convert Quaternion -> Matrix
    Matrix3f finalMat = QuatToMatrix(q);

    // Assert that initial matrix matches the final matrix elements
    for(int r = 0; r < 3; ++r) {
        for(int c = 0; c < 3; ++c) {
            assert(IsClose(initialMat(r,c), finalMat(r,c)));
        }
    }
    std::cout << "[PASS] Matrix Round-Trip Verification Passed.\n";
}

// ==========================================================================
// TEST 4: 30-Degree Rotation Around Y-Axis
// ==========================================================================
void TestRotationY30() {
    float radiansInput = static_cast<float>(M_PI / 6.0); // 30 degrees
    Vec3f axisInput(0.0f, 1.0f, 0.0f);

    Quat4f q;
    q.setAxisAngle(radiansInput, axisInput);

    // Expected: [cos(15 deg), 0, sin(15 deg), 0] -> [0.96592, 0, 0.25881, 0]
    assert(IsClose(q.w(), 0.965926f));
    assert(IsClose(q.x(), 0.0f));
    assert(IsClose(q.y(), 0.258819f));
    assert(IsClose(q.z(), 0.0f));

    // Full roundtrip via Matrix
    Matrix3f mat = QuatToMatrix(q);
    Quat4f qFromMat = Quat4f::fromRotationMatrix(mat);

    // Quaternions can flip signs and mean the same rotation (q == -q).
    // Test dot product to ensure they represent the identical orientation.
    assert(IsClose(std::fabs(Quat4f::dot(q, qFromMat)), 1.0f));

    // Extract Axis Angle back
    float radiansOutput = 0.0f;
    Vec3f axisOutput = qFromMat.getAxisAngle(&radiansOutput);

    assert(IsClose(radiansOutput, radiansInput));
    assert(IsClose(axisOutput[0], 0.0f));
    assert(IsClose(axisOutput[1], 1.0f));
    assert(IsClose(axisOutput[2], 0.0f));

    std::cout << "[PASS] 30-Degree Y-Axis Roundtrip Passed.\n";
}

// ==========================================================================
// TEST 5: 60-Degree Rotation Around Z-Axis
// ==========================================================================
void TestRotationZ60() {
    float radiansInput = static_cast<float>(M_PI / 3.0); // 60 degrees
    Vec3f axisInput(0.0f, 0.0f, 1.0f);

    Quat4f q;
    q.setAxisAngle(radiansInput, axisInput);

    // Expected: [cos(30 deg), 0, 0, sin(30 deg)] -> [0.866025, 0, 0, 0.5]
    assert(IsClose(q.w(), 0.866025f));
    assert(IsClose(q.x(), 0.0f));
    assert(IsClose(q.y(), 0.0f));
    assert(IsClose(q.z(), 0.500000f));

    // Full roundtrip via Matrix
    Matrix3f mat = QuatToMatrix(q);
    Quat4f qFromMat = Quat4f::fromRotationMatrix(mat);
    assert(IsClose(std::fabs(Quat4f::dot(q, qFromMat)), 1.0f));

    // Extract Axis Angle back
    float radiansOutput = 0.0f;
    Vec3f axisOutput = qFromMat.getAxisAngle(&radiansOutput);

    assert(IsClose(radiansOutput, radiansInput));
    assert(IsClose(axisOutput[0], 0.0f));
    assert(IsClose(axisOutput[1], 0.0f));
    assert(IsClose(axisOutput[2], 1.0f));

    std::cout << "[PASS] 60-Degree Z-Axis Roundtrip Passed.\n";
}

// ==========================================================================
// TEST 6: 45-Degree Rotation Around a Non-Aligned Complex Axis [1, 1, 1]
// ==========================================================================
void TestComplexAxis45() {
    float radiansInput = static_cast<float>(M_PI / 4.0); // 45 degrees
    
    // Testing non-unit input vector to verify the internalization normalization check
    Vec3f axisInput(1.0f, 1.0f, 1.0f); 
    float len = std::sqrt(3.0f);
    Vec3f expectedNormalizedAxis(1.0f / len, 1.0f / len, 1.0f / len);

    Quat4f q;
    q.setAxisAngle(radiansInput, axisInput);

    // Verify raw quaternion calculation consistency
    // half-angle = 22.5 deg. w = cos(22.5) = 0.923879. xyz = sin(22.5) * normalized_axis
    float sinHalf = std::sin(22.5f * static_cast<float>(M_PI) / 180.0f);
    assert(IsClose(q.w(), 0.923879f));
    assert(IsClose(q.x(), sinHalf * expectedNormalizedAxis[0]));
    assert(IsClose(q.y(), sinHalf * expectedNormalizedAxis[1]));
    assert(IsClose(q.z(), sinHalf * expectedNormalizedAxis[2]));

    // Matrix Conversion Roundtrip
    Matrix3f mat = QuatToMatrix(q);
    Quat4f qFromMat = Quat4f::fromRotationMatrix(mat);
    assert(IsClose(std::fabs(Quat4f::dot(q, qFromMat)), 1.0f));

    // Axis Angle Recovery
    float radiansOutput = 0.0f;
    Vec3f axisOutput = qFromMat.getAxisAngle(&radiansOutput);

    assert(IsClose(radiansOutput, radiansInput));
    assert(IsClose(axisOutput[0], expectedNormalizedAxis[0]));
    assert(IsClose(axisOutput[1], expectedNormalizedAxis[1]));
    assert(IsClose(axisOutput[2], expectedNormalizedAxis[2]));

    std::cout << "[PASS] 45-Degree Complex Axis [1,1,1] Roundtrip Passed.\n";
}

void TestQuat4f(){
    TestIdentityConversion();
    TestRotationX90();
    TestMatrixRoundTrip();
    TestRotationY30();
    TestRotationZ60() ;
    TestComplexAxis45();
}