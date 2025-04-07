#!/usr/bin/env python3
import redis
import numpy as np

# Connect to a local Redis server running on default port 6379
# decode_responses=False means data will be stored as bytes, not strings
r = redis.Redis(host='localhost', port=6379, decode_responses=False)

# Function to calculate cosine similarity between two vectors
def cosine_similarity(v1, v2):
    # Cosine similarity = dot product of vectors / (norm of v1 * norm of v2)
    return np.dot(v1, v2) / (np.linalg.norm(v1) * np.linalg.norm(v2))

# Clear all keys in the current Redis database to ensure a fresh start
r.flushall()

# Define a dictionary of charms, each associated with a 4-dimensional feature vector
# These vectors represent some abstract features (like color, power level, etc.)
charms = {
    "LuckyCharm": np.array([0.2, 0.9, 0.5, 0.4], dtype=np.float32),
    "GuardianCharm": np.array([0.7, 0.1, 0.3, 0.6], dtype=np.float32),
    "FortuneCharm": np.array([0.3, 0.4, 0.8, 0.7], dtype=np.float32)
}

# Store each charm and its corresponding vector in Redis
for charm_name, vector in charms.items():
    # Convert the NumPy vector to a byte string for storage in Redis
    r.set(f"charm:{charm_name}", vector.tobytes())
    print(f"Added charm: {charm_name} with vector: {vector}")

# Define a query vector to compare against the stored charm vectors
# This could represent the desired properties or features to search for
query_vector = np.array([0.25, 0.85, 0.45, 0.5], dtype=np.float32)
print(f"\nQuery Vector: {query_vector}")

# Initialize a list to store similarity results
results = []

# Compare the query vector with each charm vector
for charm_name, vector in charms.items():
    # Compute the cosine similarity between the query and charm vector
    similarity = cosine_similarity(query_vector, vector)
    # Store the result as a tuple (charm name, similarity score)
    results.append((charm_name, similarity))

# Sort the results by similarity score in descending order
results.sort(key=lambda x: x[1], reverse=True)

# Display the sorted results
print("\nSimilarity Search Results:")
for charm_name, similarity in results:
    print(f"Charm: {charm_name}, Similarity: {similarity:.4f}")
