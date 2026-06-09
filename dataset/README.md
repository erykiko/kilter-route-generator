---
license: mit
---
Try model here: https://genclimb.pages.dev/
# Kilter Board Climbing Dataset

This dataset contains climbing sequences for the Kilter Board, a popular adjustable climbing wall. It includes climbs that meet specific criteria, along with vocabulary mappings.

## Dataset Characteristics

- **Minimum Ascensionists:** 5
- **Board Layouts:** Kilter Board Original and Kilter Board Homewall
- **Quality Rating:** Greater than 2.6
- **Frames Count:** 1

## Data Format

Each sample in the dataset is structured as follows:
[Source: [Board, Difficulty], Target: [Frames]]

### Example Sample
```json
{
  "source": [1270, 1239],
  "target": [42, 1194, 156, 1194, 94, 1195, 169, 1195, 115, 1195, 201, 1195, 84, 1195, 181, 1195, 68, 1195, 184, 1196, 29, 1197, 36, 1197, 168, 1197, 206, 1197, 114, 1197, 89, 1197, 63, 1197]
}
```

### Vocabulary and Token-ID Mappings
The dataset includes tokenizer and vocabulary mappings:

- token_to_id: Maps tokens to their corresponding integer IDs.
- id_to_token: Maps integer IDs back to their corresponding tokens.

These mappings allow for efficient encoding and decoding of the climbing sequences.


### Usage
This dataset is designed for training machine learning models, particularly those focused on generating or analyzing climbing routes. The source-target structure makes it suitable for sequence-to-sequence models or other architectures that can learn from paired data.
License
This dataset is provided under the MIT License. Please refer to the license file for more details on usage and distribution.
