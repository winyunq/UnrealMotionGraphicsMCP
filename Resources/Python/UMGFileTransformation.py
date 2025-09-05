'''
Handles UMG asset transformations, such as exporting to JSON and applying JSON back to a UMG asset.
'''
from UmgMcpClient import UMGGet

class UMGFileTransformation:
    def __init__(self, client: UMGGet):
        self.client = client

    def export_umg_to_json(self, asset_path: str) -> dict:
        '''
        Exports a UMG asset to a JSON string.

        Args:
            asset_path: The asset path of the UMG widget to export.

        Returns:
            A dictionary containing the result of the operation.
        '''
        return self.client.execute_command('export_umg_to_json', {'asset_path': asset_path})

    def apply_json_to_umg(self, asset_path: str, json_data: str) -> dict:
        '''
        Applies a JSON string to a UMG asset.

        Args:
            asset_path: The asset path of the UMG widget to apply the JSON to.
            json_data: The JSON string representing the UMG data.

        Returns:
            A dictionary containing the result of the operation.
        '''
        return self.client.execute_command('apply_json_to_umg', {'asset_path': asset_path, 'json_data': json_data})
